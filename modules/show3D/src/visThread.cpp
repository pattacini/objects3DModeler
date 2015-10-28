#include "visThread.h"

using namespace std;
using namespace yarp::os;

// Empty constructor
VisThread::VisThread(int period, const string &_cloudname):RateThread(period), id(_cloudname){}

// Initialize Variables
bool VisThread::threadInit()
{
    cloud = pcl::PointCloud<pcl::PointXYZRGB>::Ptr (new pcl::PointCloud<pcl::PointXYZRGB>); // Point cloud
    cloud_normals = pcl::PointCloud<pcl::Normal>::Ptr (new pcl::PointCloud<pcl::Normal>); // Normals point cloud
    cloud_normalColors = pcl::PointCloud<pcl::PointXYZRGB>::Ptr (new pcl::PointCloud<pcl::PointXYZRGB>); // Normal Colors point cloud
    viewer = boost::shared_ptr<pcl::visualization::PCLVisualizer> (new pcl::visualization::PCLVisualizer("Point Cloud Viewer")); //viewer
    viewer->setSize(1000,650);
    viewer->setPosition(0,350);


    //initialize here variables
    printf("\nStarting visualizer Thread\n");

    // Flags
    initialized = false;
    addClouds = false;
    clearing = false;
    updatingCloud = false;
    displayBB = false;
    displayNormals = false;
    displayOMSEGI = false;
    displayHist = true;
    normalColors = false;
    normalsComputed = false;

    // Processing parameters
    styleBB = 2;
    radiusSearch = 0.01;
    resFeats = 0.01;

    return true;
}

// Module and visualization loop
void VisThread::run()
{
    if (initialized)
    {
        if (!viewer->wasStopped ())
        {
            viewer->spinOnce ();
            // Get lock on the boolean update and check if cloud was updated
            boost::mutex::scoped_lock updateLock(updateModelMutex);
            if(update)
            {
                if(updatingCloud){
                    // Clean visualizer to plot new cloud
                    viewer->removePointCloud(id);
                    viewer->removePointCloud("normals");
                    viewer->removeAllShapes();

                    // Check if the loaded file contains color information or not
                    bool colorCloud = false;
                    for  (int p = 1; p < cloud->points.size(); ++p)      {
                        uint32_t rgb = *reinterpret_cast<int*>(&cloud->points[p].rgb);
                        if (rgb != 0)
                            colorCloud = true;
                    }
                    // Add cloud color if it has not
                    if (!colorCloud)    {
                        // Define R,G,B colors for the point cloud
                        pcl::visualization::PointCloudColorHandlerCustom<pcl::PointXYZRGB> cloud_color_handler(cloud, 230, 20, 0); //red
                        viewer->addPointCloud (cloud, cloud_color_handler, id);
                    }else{
                        pcl::visualization::PointCloudColorHandlerRGBField<pcl::PointXYZRGB> cloud_color_handler (cloud);
                        viewer->addPointCloud (cloud, cloud_color_handler, id);
                    }
                    viewer->setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 2, id);
                    updatingCloud = false;
                }

                // Compute and bounding box to display
                if (displayBB)
                {
                    plotBB(styleBB);
                    displayBB = false;
                }

                // Compute and add normals to display
                if (displayNormals)
                {
                    plotNormals(radiusSearch,normalColors);
                    displayNormals = false;
                }
                
                // Compute and add normals to display
                if (displayOMSEGI)
                {
                    plotOMSEGI(resFeats, displayHist);
                    displayOMSEGI = false;
                }

                // Clear display
                if (clearing){
                    viewer->removePointCloud(id);
                    viewer->removePointCloud("normals");
                    viewer->removeAllShapes();
                    clearing = false;
                }

                update = false;
            }
            updateLock.unlock();
        }else{
            //Close viewer
            printf("Closing Visualizer\n");
            viewer->close();
            viewer->removePointCloud(id);
            this->stop();
        }

    }
}

void VisThread::threadRelease()
{
    printf("Closing Visualizer Thread\n");
}

// Unlock mutex temporarily to allow an update cycle on the visualizer
void VisThread::updateVis()
{
    // Lock mutex while update is on
    boost::mutex::scoped_lock updateLock(updateModelMutex);
    update = true;
    updateLock.unlock();

    printf("Visualizer updated\n");
}

// Clear display
void VisThread::clearVisualizer()
{
    clearing = true;
    updateVis();
}

// Display new cloud received
void VisThread::updateCloud(const pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud_in)
{
    printf("Updating displayed cloud\n");
    if (addClouds)
        *cloud += *cloud_in; // new cloud is added to last one
    else
        *cloud = *cloud_in;  // new cloud overwrites last one

    if (!initialized)
    {
        // Set camera position and orientation
        //viewer->setBackgroundColor (0.05, 0.05, 0.05, 0); // Setting background to a dark grey
        viewer->setBackgroundColor (1,1,1, 0); // Setting background to a white
        viewer->addCoordinateSystem (0.05);
        initialized = true;
    }

    updatingCloud = true;
    updateVis();
    printf("Cloud updated\n");
}

// Set flow to allow normals to be computed and added on display update.
void VisThread::addNormals(double rS, bool normCol)
{
    if (initialized){
        displayNormals = true;
        normalColors = normCol;
        radiusSearch = rS;
        updateVis();
    }else{
        printf("Please load a cloud before trying to compute the normals\n");
    }
}

// Compute and display normals
void VisThread::plotNormals(double rS, bool normCol)
{
    if (normCol)
        printf("Plotting normals as RGB colors.\n");
    else 
        printf("Plotting Vector normals.\n");
  
    // Clear clouds
    cloud_normals->points.clear();
    cloud_normalColors->points.clear();
    
    // Create the normal estimation class, and pass the input dataset to it
    pcl::NormalEstimation<pcl::PointXYZRGB, pcl::Normal> ne;
    ne.setInputCloud (cloud);

    // Create an empty kdtree representation, and pass it to the normal estimation object.
    // Its content will be filled inside the object, based on the given input dataset (as no other search surface is given).
    pcl::search::KdTree<pcl::PointXYZRGB>::Ptr tree (new pcl::search::KdTree<pcl::PointXYZRGB> ());
    ne.setSearchMethod (tree);

    // Use all neighbors in a sphere of radius radiusSearch
    ne.setRadiusSearch (rS);

    // Compute the features
    ne.compute (*cloud_normals);
    
    if (normCol)    // Plot Normals as colors on the cloud
        {
            //pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud_colorNorm(new pcl::PointCloud<pcl::PointXYZRGB> ());
            for  (int p = 1; p < cloud_normals->points.size(); ++p)      {
                if ((pcl::isFinite(cloud_normals->points[p]))){

                    // Tansform normal orientation to color
                    float xn = cloud_normals->points[p].normal_x;
                    float yn = cloud_normals->points[p].normal_y;
                    float zn = cloud_normals->points[p].normal_z;

                    // Non-Linear Transformation (cos->rgb)
                    int xn_r = (int) ((xn + 1) * 256/2);
                    int yn_g = (int) ((yn + 1) * 256/2);
                    int zn_b = (int) ((zn + 1) * 256/2);

                    // Linear Transformation (deg->rgb)
                    //float xndeg = acos(xn)*((xn > 0) - (xn < 0))*180.0/3.14;
                    //float yndeg = acos(yn)*((yn > 0) - (yn < 0))*180.0/3.14;
                    //float zndeg = acos(zn)*((zn > 0) - (zn < 0))*180.0/3.14;
                    //int xn_r = (int) ((xndeg + 180) * 256/360);
                    //int yn_g = (int) ((yndeg + 180) * 256/360);
                    //int zn_b = (int) ((zndeg + 180) * 256/360);

                    //printf("Normals X:  %f -> R: %i \n Normals Y:  %f -> G: %i \n Normals Z:  %f -> B: %i \n", xn, xn_r, yn, yn_g, zn, zn_b);

                    pcl::PointXYZRGB pointColor;
                    pointColor.x = cloud->points[p].x;
                    pointColor.y = cloud->points[p].y;
                    pointColor.z = cloud->points[p].z;
                    pointColor.r = xn_r;
                    pointColor.g = yn_g;
                    pointColor.b = zn_b;

                    cloud_normalColors->points.push_back (pointColor);
                }
            }
            viewer->removePointCloud(id);
            pcl::visualization::PointCloudColorHandlerRGBField<pcl::PointXYZRGB> rgb(cloud_normalColors);
            viewer->addPointCloud(cloud_normalColors, rgb, id);

        }else{          // Plot Normals as vectors
            viewer->addPointCloudNormals<pcl::PointXYZRGB, pcl::Normal> (cloud, cloud_normals, 10, rS, "normals");

            viewer->setPointCloudRenderingProperties (pcl::visualization::PCL_VISUALIZER_COLOR, 0.0, 1.0, 0.0, "normals");
            viewer->setPointCloudRenderingProperties (pcl::visualization::PCL_VISUALIZER_LINE_WIDTH, 3, "normals");
        }

    normalsComputed = true;    
}


// Set flow to plot OMS-EGI visualization on viewer update.
void VisThread::addOMSEGI(double res, bool plotHist)
{
     if (!normalsComputed){
        printf(" Need to compute the normals before computing OMS-EGIs" );
        return;
     }
    displayOMSEGI = true;
    displayHist = plotHist;
    resFeats = res;
    updateVis();
}


// Compute and display octree and average normals inside each voxel
void VisThread::plotOMSEGI(double res, bool plotHist)
{     
    cout << "Display OMS-EGI" << endl;

    double cBB_min_x, cBB_min_y, cBB_min_z, cBB_max_x, cBB_max_y, cBB_max_z;
    pcl::octree::OctreePointCloudSearch<pcl::PointXYZRGB> octree(res);
    octree.setInputCloud(cloud);
    octree.addPointsFromInputCloud();

    //octree.getBoundingBox(cBB_min_x, cBB_min_y, cBB_min_z, cBB_max_x, cBB_max_y, cBB_max_z);
    //octree.defineBoundingBox(cBB_min_x, cBB_min_y, cBB_min_z,cBB_max_x, cBB_max_y, cBB_max_z);
    //octree.addPointsFromInputCloud();

    // voxelCloud and voxelCloudNormals to contain the poitns on each voxel
    pcl::PointCloud<pcl::PointXYZRGB>::Ptr voxelCloud(new pcl::PointCloud<pcl::PointXYZRGB> ());
    pcl::PointCloud<pcl::Normal>::Ptr voxelCloudNormals(new pcl::PointCloud<pcl::Normal> ());
    

    // Extracts all the points at leaf level from the octree
    cout << "Setting up octree iterators" << endl;
    int voxelI = 0;
    std::stringstream voxID;
    std::stringstream sphID;
    pcl::octree::OctreePointCloudSearch<pcl::PointXYZRGB>::LeafNodeIterator tree_it;
    pcl::octree::OctreePointCloudSearch<pcl::PointXYZRGB>::LeafNodeIterator tree_it_end = octree.leaf_end();
    // Iterate through lower level leafs (voxels).

    cout << "Starting octree voxel iteration" << endl;
    for (tree_it = octree.leaf_begin(octree.getTreeDepth()); tree_it!=tree_it_end; ++tree_it)
    {

        cout << "Computing on voxel: " << voxelI << endl;

        // Clear clouds and vectors from previous voxel.
        voxelCloud->points.clear();
        voxelCloudNormals->points.clear();

        // Find Voxel Bounds ...
        Eigen::Vector3f voxel_min, voxel_max;
        octree.getVoxelBounds(tree_it, voxel_min, voxel_max);

        //... and center
        pcl::PointXYZRGB voxelCenter;
        voxelCenter.x = (voxel_min.x() + voxel_max.x()) / 2.0f;
        voxelCenter.y = (voxel_min.y() + voxel_max.y()) / 2.0f;
        voxelCenter.z = (voxel_min.z() + voxel_max.z()) / 2.0f;
        
        // ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
        // Neighbors within voxel search
        // It assigns the search point to the corresponding leaf node voxel and returns a vector of point indices.
        // These indices relate to points which fall within the same voxel.
        cout << "Saving Normals in voxel " << voxelI << endl;
        std::vector<int> voxelPointsIdx;
        if (octree.voxelSearch (voxelCenter, voxelPointsIdx))
        {
            for (size_t i = 0; i < voxelPointsIdx.size (); ++i)
            {
                // Save Points within voxel as cloud
                voxelCloud->points.push_back(cloud->points[voxelPointsIdx[i]]);
                voxelCloudNormals->points.push_back(cloud_normals->points[voxelPointsIdx[i]]);
            } // for point in voxel
        }
        std::cout << "Voxel has " << voxelCloud->points.size() << " points. " << std::endl;

        cout << "Computing voxel normal average " << endl;
        //cout << "Getting points from voxel " << voxelI << endl;
        float xn_Acc = 0;
        float yn_Acc = 0;
        float zn_Acc = 0;
        int pointsInVoxel = voxelCloud->points.size();
        for (size_t i = 0; i < voxelCloudNormals->points.size (); ++i)
        {
            float xn, yn, zn;
            if ((pcl::isFinite(voxelCloudNormals->points[i])))
            {
                xn = voxelCloudNormals->points[i].normal_x;
                yn = voxelCloudNormals->points[i].normal_y;
                zn = voxelCloudNormals->points[i].normal_z;
                //cout<< "Point " << pointsInVoxel << " has Normals " << xn << ", " << yn << ", "<< zn << ". "<< endl;

                xn_Acc = xn_Acc + xn;
                yn_Acc = yn_Acc + yn;
                zn_Acc = zn_Acc + zn;
                //cout<< "Point " << pointsInVoxel << " has accum  Normals " << xn_Acc << ", " << yn_Acc << ", "<< zn_Acc << ". "<< endl;
            }
        }
        float xn_avg = xn_Acc/pointsInVoxel;
        float yn_avg = yn_Acc/pointsInVoxel;
        float zn_avg = zn_Acc/pointsInVoxel;

        float xn_r = (xn_avg + 1) /2;
        float yn_g = (yn_avg + 1) /2;
        float zn_b = (zn_avg + 1) /2;

        //printf("Normals Accum X: %g Avg X:  %f -> R: %f \n", xn_Acc, xn_avg, xn_r);
        //printf("Normals Accum Y: %f Avg Y:  %f -> G: %f \n", yn_Acc, yn_avg, yn_g);
        //printf("Normals Accum Z: %f Avg Z:  %f -> B: %f \n", zn_Acc, zn_avg, zn_b);
        //printf("for %i points.\n",pointsInVoxel);

        // Display a cube for each considered voxel
        voxID << "vox" << voxelI;
        sphID << "sph" << voxelI;

        cout << "Dsiplaying cube on box " <<  endl;
        viewer->addCube(voxelCenter.x-res/2, voxelCenter.x+res/2, voxelCenter.y-res/2, voxelCenter.y+res/2, voxelCenter.z-res/2, voxelCenter.z+res/2, xn_r, yn_g, zn_b,voxID.str());
        if (plotHist){
            viewer->addSphere(voxelCenter, res/4, xn_r, yn_g, zn_b, sphID.str());
        }
        voxelI++;
    }
    viewer->removePointCloud(id);
}

// Set flow to allow bounding box to be computed and added on display update.
void VisThread::addBoundingBox(int typeBB)
{
    if (initialized){
        displayBB = true;
        styleBB = typeBB;

        updateVis();

        if (typeBB == 0)
            printf("Minimal Bounding Box added to display\n");
        else if (typeBB == 1)
            printf("Axis Aligned Bounding Box added to display\n");
        else if (typeBB == 2)
            printf("Cubic Axis Aligned Bounding Box added to display\n");
    }else{
        printf("Please load cloud to compute Bounding box from.\n");
    }
}

// Computes and display Bounding Box
void VisThread::plotBB(int typeBB)
{
    // Create the feature extractor estimation class, and pass the input cloud to it
    pcl::MomentOfInertiaEstimation <pcl::PointXYZRGB> feature_extractor;
    feature_extractor.setInputCloud (cloud);
    feature_extractor.compute ();

    if (typeBB == 0)          // Oriented bounding box (OBB)
    {   // Declare points and rotational matrix
        pcl::PointXYZRGB min_point_OBB;
        pcl::PointXYZRGB max_point_OBB;
        pcl::PointXYZRGB position_OBB;
        Eigen::Matrix3f rotational_matrix_OBB;

        // Compute extrema of minimum bounding box and rotational matrix wrt axis.
        feature_extractor.getOBB (min_point_OBB, max_point_OBB, position_OBB, rotational_matrix_OBB);
        Eigen::Vector3f position (position_OBB.x, position_OBB.y, position_OBB.z);
        Eigen::Quaternionf quat (rotational_matrix_OBB);

        // Display oriented minimum boinding box
        viewer->addCube (position, quat, max_point_OBB.x - min_point_OBB.x, max_point_OBB.y - min_point_OBB.y, max_point_OBB.z - min_point_OBB.z, "OBB");

    }else if (typeBB == 1){              // Axis-aligned bounding box (AABB)
        // Declare extrema points of AABB
        pcl::PointXYZRGB min_point_AABB;
        pcl::PointXYZRGB max_point_AABB;

        // Compute extrema of AABB
        feature_extractor.getAABB (min_point_AABB, max_point_AABB);

        // Display axis aligned minimum boinding box
        viewer->addCube (min_point_AABB.x, max_point_AABB.x, min_point_AABB.y, max_point_AABB.y, min_point_AABB.z, max_point_AABB.z, 0,0,0, "AABB");
    }else if (typeBB == 2){              // Cubic Axis Aligned Boundinb Box CAABB.
        pcl::MomentOfInertiaEstimation <pcl::PointXYZRGB> feature_extractor;
        feature_extractor.setInputCloud (cloud);
        feature_extractor.compute ();

        pcl::PointXYZRGB min_point_AABB;
        pcl::PointXYZRGB max_point_AABB;
        //pcl::PointXYZRGB min_point_CAABB;
        //pcl::PointXYZRGB max_point_CAABB;
        double mx,my,mz,MX,MY,MZ;
        feature_extractor.getAABB (min_point_AABB, max_point_AABB);

        pcl::octree::OctreePointCloudSearch<pcl::PointXYZRGB> octree(resFeats);
        octree.setInputCloud(cloud);
        octree.addPointsFromInputCloud();
        octree.defineBoundingBox(min_point_AABB.x, min_point_AABB.y, min_point_AABB.z,
                                 max_point_AABB.x, max_point_AABB.y, max_point_AABB.z);
        octree.getBoundingBox(mx,my,mz,MX,MY,MZ);

        cout << "AA BB: (" << min_point_AABB.x << "," << min_point_AABB.y << "," << min_point_AABB.z << "), (" << max_point_AABB.x << "," <<max_point_AABB.y << "," << max_point_AABB.z << ")" << endl;
        cout << "Octree BB: (" << mx << "," << my << "," << mz << "), (" << MX << "," << MY << "," << MZ << ")" << endl;


        //viewer->addCube(min_point_CAABB.x, max_point_CAABB.x, min_point_CAABB.y, max_point_CAABB.y, min_point_CAABB.z, max_point_CAABB.z, 1, 1, 1,"CAABB");
        viewer->addCube(mx,MX,my,MY,mz,MZ, 1, 0, 0,"CAABB");
    }
}

// Selects between displaying each cloud individually or accumulating them on the display.
void VisThread::accumulateClouds(bool accum)
{
    if (accum){
        addClouds = true;
        printf("Clouds plotted together now\n");
    }else{
        addClouds = false;
        printf("Clouds plotted separately now\n");
    }
}
