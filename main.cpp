#include <opencv2/opencv.hpp>

using namespace std;
using namespace cv;

#define EPSILON 0.0000001

pair<Point,double> circleFromPoints(Point p1, Point p2, Point p3);
double dist(Point x, Point y);
double square(double n);

int main(int argc, char *argv[])  {
	Mat frame;
	Mat back;
	Mat fore;
    Mat captured;

	vector<pair<Point,double> > palm_centers;
	VideoCapture cap(0);

    cv::Ptr<BackgroundSubtractorMOG2> bg = createBackgroundSubtractorMOG2();
    bg->setNMixtures(3);
    bg->setDetectShadows(false);

    namedWindow("Frame");
	namedWindow("Background");

	int backgroundFrame = 500;

    char buffer[64];

	while( 1 ) {
		vector<vector<Point> > contours;

		//Get the frame
		cap >> captured; //frame;

        flip(captured, frame, 1);

		//Update the current background model and get the foreground
		if( backgroundFrame > 0 )  {
            bg->apply(frame, fore); //bg.operator ()(frame,fore);
            backgroundFrame--;
        }
		else {
            bg->apply(frame, fore, 0);
        }

		//Get background image to display it
        bg->getBackgroundImage(back); //bg.getBackgroundImage(back);


		//Enhance edges in the foreground by applying erosion and dilation
		erode(fore, fore, Mat());
		dilate(fore, fore, Mat());

		//Find the contours in the foreground
		findContours(fore,contours,CV_RETR_EXTERNAL,CV_CHAIN_APPROX_NONE);

		for(int i = 0; i < contours.size(); i++) {
			//Ignore all small insignificant areas
			if (contourArea(contours[i]) >= 5000) {
				//Draw contour
				vector<vector<Point> > tcontours;
				tcontours.push_back(contours[i]);
				drawContours(frame, tcontours, -1, cv::Scalar(0, 0, 255), 2);

				//Detect Hull in current contour
				vector<vector<Point> > hulls(1);
				vector<vector<int> > hullsI(1);
				convexHull(Mat(tcontours[0]), hulls[0], false);
				convexHull(Mat(tcontours[0]), hullsI[0], false);
				drawContours(frame, hulls, -1, cv::Scalar(0, 255, 0), 2);

				//Find minimum area rectangle to enclose hand
				RotatedRect rect = minAreaRect(Mat(tcontours[0]));

				//Find Convex Defects
				vector<Vec4i> defects;
				if (hullsI[0].size() > 0) {
					Point2f rect_points[4];
					rect.points(rect_points);

					for (int j = 0; j < 4; j++)
						line(frame, rect_points[j], rect_points[(j + 1) % 4], Scalar(255, 0, 0), 1, 8);

					Point rough_palm_center;
					convexityDefects(tcontours[0], hullsI[0], defects);
					if (defects.size() >= 3) {
						vector<Point> palm_points;

						for (int j = 0; j < defects.size(); j++) {
							int startidx = defects[j][0];
							Point ptStart(tcontours[0][startidx]);
							int endidx = defects[j][1];
							Point ptEnd(tcontours[0][endidx]);
							int faridx = defects[j][2];
							Point ptFar(tcontours[0][faridx]);
							//Sum up all the hull and defect points to compute average
							rough_palm_center += ptFar + ptStart + ptEnd;
							palm_points.push_back(ptFar);
							palm_points.push_back(ptStart);
							palm_points.push_back(ptEnd);
						}

						//Get palm center by 1st getting the average of all defect points, this is the rough palm center,
						//Then U chose the closest 3 points ang get the circle radius and center formed from them which is the palm center.
						rough_palm_center.x /= defects.size() * 3;
						rough_palm_center.y /= defects.size() * 3;

						//Point closest_pt = palm_points[0];

						vector<pair<double, int> > distvec;

						for (int k = 0; k < palm_points.size(); k++)
							distvec.push_back(make_pair(dist(rough_palm_center, palm_points[k]), k)); // ir or k?

						sort(distvec.begin(), distvec.end());

						//Keep choosing 3 points till you find a circle with a valid radius
						//As there is a high chance that the closes points might be in a linear line or too close that it forms a very large circle
						pair<Point, double> soln_circle;

						for (int k = 0; k + 2 < distvec.size(); k++) {
							Point p1 = palm_points[distvec[k + 0].second];
							Point p2 = palm_points[distvec[k + 1].second];
							Point p3 = palm_points[distvec[k + 2].second];

							soln_circle = circleFromPoints(p1, p2, p3);//Final palm center,radius

							if (soln_circle.second != 0)
								break;
						}

						//Find avg palm centers for the last few frames to stabilize its centers, also find the avg radius
						palm_centers.push_back(soln_circle);
						
						if (palm_centers.size() > 10)
							palm_centers.erase(palm_centers.begin());

						Point palm_center;
						double radius = 0;

						for (int k = 0; k < palm_centers.size(); k++) {
							palm_center += palm_centers[k].first;
							radius += palm_centers[k].second;
						}

						palm_center.x /= palm_centers.size();
						palm_center.y /= palm_centers.size();
						radius /= palm_centers.size();

						//Draw the palm center and the palm circle
						//The size of the palm gives the depth of the hand
						circle(frame, palm_center, 5, Scalar(144, 144, 255), 3);
						circle(frame, palm_center, (int)radius, Scalar(144, 144, 255), 2);

						//Detect fingers by finding points that form an almost isosceles triangle with certain thesholds
						int no_of_fingers = 0;
						for (int j = 0; j < defects.size(); j++) {
							int startidx = defects[j][0];
							Point ptStart(tcontours[0][startidx]);
							int endidx = defects[j][1];
							Point ptEnd(tcontours[0][endidx]);
							int faridx = defects[j][2];
							Point ptFar(tcontours[0][faridx]);
							//X o--------------------------o Y
							double Xdist = sqrt(dist(palm_center, ptFar));
							double Ydist = sqrt(dist(palm_center, ptStart));
							double length = sqrt(dist(ptFar, ptStart));

							double retLength = sqrt(dist(ptEnd, ptFar));
							//Play with these thresholds to improve performance
							if (length <= 3 * radius && Ydist >= 0.4 * radius && length >= 10 && retLength >= 10 &&
								max(length, retLength) / min(length, retLength) >= 0.8)
								if (min(Xdist, Ydist) / max(Xdist, Ydist) <= 0.8) {
									if ((Xdist >= 0.1 * radius && Xdist <= 1.3 * radius && Xdist < Ydist) ||
										(Ydist >= 0.1 * radius && Ydist <= 1.3 * radius && Xdist > Ydist))
										line(frame, ptEnd, ptFar, Scalar(0, 255, 0), 1), no_of_fingers++;
								}


						}

						no_of_fingers = min(5, no_of_fingers);
						cout << "NO OF FINGERS: " << no_of_fingers << endl;


					}
				}

			}

		}

		if( backgroundFrame > 0) {
            snprintf(buffer, 50, "Recording Background: %d", backgroundFrame);

            putText(frame, buffer, cvPoint(30, 30), FONT_HERSHEY_COMPLEX_SMALL, 0.8,
                    cvScalar(200, 200, 250), 1, CV_AA);
        }

        imshow("Frame", frame);
		imshow("Background", back);

		if( waitKey(10) == 'x' )
            break;
	}


	return 0;
}

pair<Point,double> circleFromPoints(Point p1, Point p2, Point p3)
{
    double offset = p2.x * p2.x + p2.y * p2.y;
    double bc =   (p1.x * p1.x + p1.y * p1.y - offset ) / 2.0;
    double cd =   (offset - p3.x * p3.x - p3.y * p3.y) / 2.0;
    double det =  (p1.x - p2.x) * (p2.y - p3.y) - (p2.x - p3.x)* (p1.y - p2.y);

    if ( abs(det) < EPSILON ) {
        cout<<"POINTS TOO CLOSE"<< endl;

        return make_pair(Point(0,0),0);
    }

    double idet = 1/det;
    double centerx =  (bc * (p2.y - p3.y) - cd * (p1.y - p2.y)) * idet;
    double centery =  (cd * (p1.x - p2.x) - bc * (p2.x - p3.x)) * idet;
    double radius = sqrt(square(p2.x - centerx) + square(p2.y - centery));

    return make_pair(Point((int)centerx, (int)centery), radius);
}

double dist(Point x, Point y) {
    return (x.x - y.x) * (x.x - y.x) + (x.y - y.y) * (x.y - y.y);
}

double square(double n) {
	return n * n;
}