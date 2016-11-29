
#include "toolRecognizer.h"

using namespace std;
using namespace yarp::os;
using namespace yarp::sig;

/**********************************************************
                    PRIVATE METHODS
/**********************************************************/



/*******************************************************************************/
bool ToolRecognizer::trainObserve(const string &label, BoundingBox &bb)
{
    ImageOf<PixelRgb> img= *portImgIn.read(true);
    portImgOut.write(img);

    Bottle bot;
    // Check if Bounding box was not initialized
    if ((bb.tlx + bb.tly + bb.brx + bb.bry) == 0.0 ){
        bot = *portBBcoordsIn.read(true);            // read all bounding boxes
        Bottle *items=bot.get(0).asList();                  // pick up first bounding box

        bb.tlx = items->get(0).asDouble();
        bb.tly = items->get(1).asDouble();
        bb.brx = items->get(2).asDouble();
        bb.bry = items->get(3).asDouble();
    }else{

        Bottle &items = bot.addList();
        items.addDouble(bb.tlx);
        items.addDouble(bb.tly);
        items.addDouble(bb.brx);
        items.addDouble(bb.bry);
    }

    yInfo("[trainObserve] got bounding Box is %i %i %i %i", bb.tlx, bb.tly, bb.brx, bb.bry);

    Bottle cmdHR,replyHR;
    if (burstTrain){
        cmdHR.clear();	replyHR.clear();
        cmdHR.addString("burst");
        cmdHR.addString("start");
        cout <<"Sending burst training request:  " << cmdHR.toString() <<endl;
        portHimRep.write(cmdHR,replyHR);

        for (int i=0; i < 10 ; i++){
            cmdHR.clear();	replyHR.clear();
            cmdHR.addVocab(Vocab::encode("train"));

            Bottle &options=cmdHR.addList().addList();
            options.addString(label.c_str());
            options.add(bot.get(0));

            yInfo("[trainObserve] Sending training request: %s\n",cmdHR.toString().c_str());
            portHimRep.write(cmdHR,replyHR);
            yInfo("[trainObserve] Received reply: %s\n",replyHR.toString().c_str());
        }


        cmdHR.clear();	replyHR.clear();
        cmdHR.addString("burst");
        cmdHR.addString("stop");
        cout <<"Sending burst training request:  " << cmdHR.toString() <<endl;
        portHimRep.write(cmdHR,replyHR);

    }else{

        cmdHR.clear();	replyHR.clear();
        cmdHR.addVocab(Vocab::encode("train"));

        Bottle &options=cmdHR.addList().addList();
        options.addString(label.c_str());
        options.add(bot.get(0));

        yInfo("[trainObserve] Sending training request: %s\n",cmdHR.toString().c_str());
        portHimRep.write(cmdHR,replyHR);
        yInfo("[trainObserve] Received reply: %s\n",replyHR.toString().c_str());

    }

    return true;
}



/**********************************************************/
bool ToolRecognizer::classifyObserve(string &label, BoundingBox &bb)
{
    ImageOf<PixelRgb> img= *portImgIn.read(true);
    portImgOut.write(img);

    Bottle cmd,reply;
    cmd.addVocab(Vocab::encode("classify"));

    Bottle &options=cmd.addList();                  // cmd = classify ()

    // Check if Bounding box was not initialized
    if ((bb.tlx + bb.tly + bb.brx + bb.bry) == 0.0 ){
        Bottle bot = *portBBcoordsIn.read(true);        // Read all Bounding boxes. Bot = ((bb1)(bb2)(bb3))
        for (int i=0; i<bot.size(); i++)                // Add each of them
        {
            ostringstream tag;
            tag<<"blob_"<<i;
            Bottle &item=options.addList();             // cmd = classify (() () () )
            item.addString(tag.str().c_str());          // cmd = classify ((blob_i) (blob_j) (...) )
            item.addList()=*bot.get(i).asList();        // cmd = classify ((blob_i (bb1)) (blob_j (bb2)) (...) )
        }
    }else{
        Bottle &item=options.addList();                 // cmd = classify (())
        item.addString("blob_0");                       // cmd = classify ((blob_i))
        Bottle &bbox = item.addList();                  // cmd = classify ((blob_i ()))
        bbox.addDouble(bb.tlx);
        bbox.addDouble(bb.tly);
        bbox.addDouble(bb.brx);
        bbox.addDouble(bb.bry);                         // cmd = classify ((blob_i(bb1)))
    }

    yInfo("[classifyObserve] Sending classification request: %s\n",cmd.toString().c_str());
    portHimRep.write(cmd,reply);
    yInfo("[classifyObserve] Received reply: %s\n",reply.toString().c_str());

    label = processScores(reply);

    yInfo("[classifyObserve] the recognized label is %s", label.c_str());

    return true;
}
/**********************************************************/
string ToolRecognizer::processScores(const Bottle &scores)
{

    double maxScoreObj=0.0;
    string label  ="";

    for (int i=0; i<scores.size(); i++)
    {
        ostringstream tag;
        tag<<"blob_"<<i;

        Bottle *blobScores=scores.find(tag.str().c_str()).asList();
        if (blobScores==NULL)
            continue;

        for (int j=0; j<blobScores->size(); j++)
        {
            Bottle *item=blobScores->get(j).asList();
            if (item==NULL)
                continue;

            string name=item->get(0).asString().c_str();
            double score=item->get(1).asDouble();

            yInfo("name is %s with score %f", name.c_str(), score);

            if (score>maxScoreObj)
            {
                maxScoreObj = score;
                label.clear();
                label = name;
            }

        }
    }
    return label;
}

/*******************************************************************************/
bool ToolRecognizer::configure(ResourceFinder &rf)
{
    name = rf.check("name", Value("toolRecognizer"), "Getting module name").asString();
    running = true;
    burstTrain = rf.check("burst", Value(true)).asBool();

    printf("Opening ports\n" );
    bool ret= true;

    ret = ret && portBBcoordsIn.open("/"+name+"/bb:i");
    ret = ret && portImgIn.open("/"+name+"/img:i");
    ret = ret && portImgOut.open("/"+name+"/img:o");

    ret = ret && portHimRep.open("/"+name+"/himrep:rpc");
    ret = ret && portRpc.open("/"+name+"/rpc:i");

    if (!ret){
        printf("Problems opening ports\n");
        return false;
    }
    printf("Ports opened\n");

    attach(portRpc);

    return true;
}

/*******************************************************************************/
bool ToolRecognizer::interruptModule()
{
    portBBcoordsIn.interrupt();
    portImgIn.interrupt();
    portImgOut.interrupt();

    portHimRep.interrupt();
    portRpc.interrupt();
    return true;
}

/*******************************************************************************/
bool ToolRecognizer::close()
{
    portBBcoordsIn.close();
    portImgIn.close();
    portImgOut.close();

    portHimRep.close();
    portRpc.close();
    return true;
}

/*******************************************************************************/
double ToolRecognizer::getPeriod()
{
    return 0.1;
}

/*******************************************************************************/
bool ToolRecognizer::updateModule()
{
    if (!running)
        return false;
    return true;
}


/* =========================================== RPC COMMANDS ========================================= */

/******************RPC HANDLING METHODS*******************/
bool ToolRecognizer::attach(yarp::os::RpcServer &source)
{
    return this->yarp().attachAsServer(source);
}

/* Atomic commands */
// Setting up commands
bool ToolRecognizer::start(){
    std::cout << "Starting!" << std::endl;
    running = true;
    return true;
}
bool ToolRecognizer::quit(){
    std::cout << "Quitting!" << std::endl;
    running = false;
    return true;
}

//Thrifted
bool ToolRecognizer::train(const string &label, const int tlx ,const int tly, const int brx, const int bry)
{

    cout << "Received command 'train'  on bb " << tlx << ", " << tly << "; "  << brx << ", " << bry << ". "<< endl;
    BoundingBox bb;
    bb.tlx = tlx;
    bb.tly = tly;
    bb.brx = brx;
    bb.bry = bry;

    return trainObserve(label, bb);
}

string ToolRecognizer::recognize(const int tlx ,const int tly, const int brx, const int bry)
{
    string label;
    cout << "Received command 'recognize'  on bb " << tlx << ", " << tly << "; "  << brx << ", " << bry << ". "<< endl;

    BoundingBox bb;
    bb.tlx = tlx;
    bb.tly = tly;
    bb.brx = brx;
    bb.bry = bry;

    cout << "Classifying image from crop " << tlx <<", "<< tly <<", "<< brx <<", "<< bry <<". "<<endl;

    classifyObserve(label, bb);

    cout << " Tool classified as: " << label << endl;

    return label;
}

//Thrifted
bool ToolRecognizer::burst(const bool burstF)
{
    burstTrain = burstF;
    if (burstF)
        cout << "Burst is now ON" << endl;
    else
        cout << "Burst is now OFF" << endl;
    return true;
}


/*******************************************************************************/
int main(int argc,char *argv[])
{   
    Network yarp;
    if (!yarp.checkNetwork())
    {
        yError("unable to find YARP server!");
        return 1;
    }

    ToolRecognizer toolRecognizer;
    ResourceFinder rf;
    rf.setDefaultContext("toolRecognizer");
    rf.configure(argc,argv);
    return toolRecognizer.runModule(rf);
}

