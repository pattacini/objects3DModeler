/*
 * Copyright (C) 2015 iCub Facility - Istituto Italiano di Tecnologia
 * Author: Ugo Pattacini, Tanis Mar
 * email:  ugo.pattacini@iit.it, tanis.mar@iit.it
 * Permission is granted to copy, distribute, and/or modify this program
 * under the terms of the GNU General Public License, version 2 or any
 * later version published by the Free Software Foundation.
 *
 * A copy of the license can be found at
 * http://www.robotcub.org/icub/license/gpl.txt
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details
*/

#ifndef __TOOLRECOGNIZER_H__
#define __TOOLRECOGNIZER_H__

#include <vector>
#include <algorithm>
#include <string>
#include <fstream>
#include <iomanip>

#include <yarp/os/all.h>
#include <yarp/sig/all.h>
#include <yarp/math/Math.h>

#include <toolRecognizer_IDLServer.h>
#include <BoundingBox.h>

class ToolRecognizer : public yarp::os::RFModule, public toolRecognizer_IDLServer
{
protected:
    yarp::os::BufferedPort<yarp::sig::ImageOf<yarp::sig::PixelRgb> > portImgIn;
    yarp::os::Port                                  portImgOut;
    yarp::os::BufferedPort<yarp::os::Bottle>        portBBcoordsIn;

    //Port portContour;
    yarp::os::RpcClient portHimRep;
    yarp::os::RpcServer portRpc;

    std::string                         name;               //name of the module
    bool                                running;

    /*****  Methods  ****/   
    bool                trainObserve(const std::string &label, BoundingBox &bb);
    bool                classifyObserve(std::string &label, BoundingBox &bb);
    std::string         processScores(const yarp::os::Bottle &scores);

public:
    // module control commands
        bool						attach(yarp::os::RpcServer &source);
        bool						start();
        bool						quit();

        bool						configure(yarp::os::ResourceFinder &rf);
        bool						interruptModule();
        bool						close();
        bool						updateModule();
        double						getPeriod();

        bool                        train(const std::string &label, const int tlx = 0.0,const int tly = 0.0, const int brx = 0.0, const int bry = 0.0);
        std::string                 recognize(const int tlx = 0.0,const int tly = 0.0, const int brx = 0.0, const int bry = 0.0);


};

#endif
