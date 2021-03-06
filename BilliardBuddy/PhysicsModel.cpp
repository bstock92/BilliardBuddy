#include "PhysicsModel.h"
#include "TrajectoryAugmentor.h"
#include <cmath>
#include <iostream>
using namespace std;

PhysicsModel::PhysicsModel()
{
}

PhysicsModel::~PhysicsModel()
{
}

cv::vector<Path> PhysicsModel::calculate(cv::Mat frame, cv::vector<pocket> pockets, cv::vector<cv::Vec2i> cueStick,
	cv::vector<cv::Vec2i> cueBall, cv::vector<cv::Vec2f> targetBalls)
{

	//Initialize path vector to be returned (trajectory points in 3D space)
	cv::vector<Path> trajectoryPoints3D;
	int B = targetBalls.size();
	//cout << targetBalls[0][0] << "____" << targetBalls[0][1] << endl;
	//Need 4 pockets to do warp perspective transform
	if ((pockets.size() == 4)&&(cueStick.size()==2)&&(cueBall.size()==1)&&(norm(cueStick[0],cueStick[1])>0.00001)){

		cv::vector<cv::Point2f> sourcePoints(4);
		cv::vector<cv::Point2f> destPoints(4);

		//Form four corners in both the source and destination spaces
		for (int i = 0; i < 4; i++){
			sourcePoints[i] = cv::Point2f(pockets[i].pocketPoints.pt.x, pockets[i].pocketPoints.pt.y);
			destPoints[i] = cv::Point2f(pockets[i].xLocation, pockets[i].yLocation);
		}
		
		//// Get mass center
		//cv::Point2f center(0, 0);
		//for (int i = 0; i < sourcePoints.size(); i++)
		//	center += sourcePoints[i];

		//center *= (1. / sourcePoints.size());
		//sortCorners(sourcePoints, center);

		//Compute transformation matrix for 3D --> 2D and 2D --> 3D respectively
		cv::Mat forwardMatrix = cv::getPerspectiveTransform(sourcePoints, destPoints);
		cv::Mat backwardMatrix = cv::getPerspectiveTransform(destPoints, sourcePoints);

		///Convert input coordinates for cue stick into floats //TEMP
		cv::vector<cv::Vec2f> cueStickf(cueStick.size());
		cv::vector<cv::Vec2f> cueBallf(cueBall.size());
		cv::vector<cv::Vec2f> targetBallsf(targetBalls.size());
		cueStickf[0][0] = (float)cueStick[0][0];
		cueStickf[0][1] = (float)cueStick[0][1];
		cueStickf[1][0] = (float)cueStick[1][0];
		cueStickf[1][1] = (float)cueStick[1][1];
		cueBallf[0][0] = (float)cueBall[0][0];
		cueBallf[0][1] = (float)cueBall[0][1];
		for (int i = 0; i < B; i++){
			targetBallsf[i][0] = (float)targetBalls[i][0];
			targetBallsf[i][1] = (float)targetBalls[i][1];
		}

		if (cueStickf[0][1]<cueStickf[1][1]){
			swapCue(cueStickf);
		}

		// Convert coordinates from Vec2f to Point2f
		cv::vector<cv::Point2f> cueStickp(2);
		cv::vector<cv::Point2f> cueBallp(1);
		cv::vector<cv::Point2f> targetBallsp(B);
		cueBallp[0] = cv::Point2f(cueBallf[0][0], cueBallf[0][1]);
		cueStickp[0] = cv::Point2f(cueStickf[0][0], cueStickf[0][1]);
		cueStickp[1] = cv::Point2f(cueStickf[1][0], cueStickf[1][1]);
		for (int b = 0; b < B; b++){
			targetBallsp[b] = cv::Point2f(targetBalls[b][0], targetBalls[b][1]);
		}

		// Obtain 2D coordinates from forward transform
		cv::vector<cv::Point2f> cueStickp2D(2);
		cv::vector<cv::Point2f> cueBallp2D(1);
		cv::vector<cv::Point2f> targetBallsp2D(B);
		cv::perspectiveTransform(cueStickp, cueStickp2D, forwardMatrix);
		cv::perspectiveTransform(cueBallp, cueBallp2D, forwardMatrix);
		cv::perspectiveTransform(targetBallsp, targetBallsp2D, forwardMatrix);

		// Convert coordinates from Point2f to Vec2f
		cv::vector<cv::Vec2f> cueStick2D(2);
		cv::vector<cv::Vec2f> cueBall2D(1);
		cv::vector<cv::Vec2f> targetBalls2D(B);
		cueBall2D[0] = {cueBallp2D[0].x,cueBallp2D[0].y};
		cueStick2D[0] = { cueStickp2D[0].x,cueStickp2D[0].y };
		cueStick2D[1] = { cueStickp2D[1].x,cueStickp2D[1].y };
		for (int b = 0; b < B; b++){
			targetBalls2D[b] = {targetBallsp2D[b].x,targetBallsp2D[b].y};
		}

		// Adjust coordinate system for that of the trajectory calculations
		adjustCoordinatesInput(cueStick2D,cueBall2D,targetBalls2D);

		// Wizard of Oz target ball
		targetBalls2D[0] = { 211.5, 225 };

		// Adjust cue stick to hit center of cue ball
		adjustCueStick(cueStick2D, cueBall2D);

		// Draw trajectories if balls are within the region
		cv::vector<Path> trajectoryPoints2D;
		int T = 0;
		if ((cueBall2D[0][0] >= 0) && (cueBall2D[0][0] <= 360) && (cueBall2D[0][1] >= 0) && (cueBall2D[0][1] <= 360)){ // temp fix (only works for one target ball)
			calculateTrajectories(trajectoryPoints2D, cueBall2D, targetBalls2D, cueStick2D);
			T = trajectoryPoints2D.size();
		}
		
		// If trajectories were calculated, draw them
		if (T != 0){

			// Bring trajectory points into original 2D coordinate frame
			adjustCoordinatesOutput(trajectoryPoints2D);

			// Shows 2D perspective transformed video with 2D trajectories
			cv::Mat rotated;
			cv::Size rSize = { 360, 360 };
			cv::warpPerspective(frame, rotated, forwardMatrix, rSize);
			TrajectoryAugmentor trajectoryAugmentor = TrajectoryAugmentor();
			trajectoryAugmentor.augment(rotated, trajectoryPoints2D);
			imshow("4 Point Warped Table", rotated);

			//Convert output 2D trajectory coordinates into points
			cv::vector<cv::Point2f> trajectoryPoints2D_StartPoints(T);
			cv::vector<cv::Point2f> trajectoryPoints2D_EndPoints(T);
			for (int t = 0; t < T; t++){
				cv::Vec2f tempStart = trajectoryPoints2D[t].startPoint;
				cv::Vec2f tempEnd = trajectoryPoints2D[t].endPoint;
				trajectoryPoints2D_StartPoints[t] = cv::Point2f(tempStart[0], tempStart[1]);
				trajectoryPoints2D_EndPoints[t] = cv::Point2f(tempEnd[0], tempEnd[1]);
			}

			//Transform the 2D trajectory coordinates back into the 3D space
			cv::vector<cv::Point2f> trajectoryPoints3D_StartPoints(T);
			cv::vector<cv::Point2f> trajectoryPoints3D_EndPoints(T);
			cv::perspectiveTransform(trajectoryPoints2D_StartPoints, trajectoryPoints3D_StartPoints, backwardMatrix);
			cv::perspectiveTransform(trajectoryPoints2D_EndPoints, trajectoryPoints3D_EndPoints, backwardMatrix);

			//Convert output 3D trajectory coordinates into Path
			//cv::vector<Path> trajectoryPoints3D(trajectoryPoints3D.size());
			for (int i = 0; i < T; i++){
				trajectoryPoints3D.push_back({ 0, 0 });
				trajectoryPoints3D[i].startPoint = { trajectoryPoints3D_StartPoints[i].x, trajectoryPoints3D_StartPoints[i].y };
				trajectoryPoints3D[i].endPoint = { trajectoryPoints3D_EndPoints[i].x, trajectoryPoints3D_EndPoints[i].y };
			}

		}
	
	}

	return trajectoryPoints3D;

}

void PhysicsModel::adjustCueStick(cv::vector<cv::Vec2f>& cueCoords, cv::vector<cv::Vec2f> ballCoord){
	
	float cueBiasX = cueCoords[0][0] - cueCoords[1][0]; //distance and direction that cue back x is from cue front x
	float cueBiasY = cueCoords[0][1] - cueCoords[1][1]; //distance and direction that cue back y is from cue front y
	float ratio = (2*ballRadius) / (norm(cueCoords[0],cueCoords[1]));
	
	cv::Vec2f ballBias;
	float slope;
	float ballBiasX;
	float ballBiasY;
	if (cueBiasX == 0){
		ballBiasX = 0;
		ballBiasY = - 2 * ballRadius;
	}
	else{
		ballBiasX = ratio*cueBiasX;
		ballBiasY = ratio*cueBiasY;
	}
	ballBias = {ballBiasX,ballBiasY};
	cueCoords[1] = ballCoord[0] + ballBias;

	//Place cue back point
	cueCoords[0][0] = cueCoords[1][0] + cueBiasX;
	cueCoords[0][1] = cueCoords[1][1] + cueBiasY;

}

void PhysicsModel::adjustCoordinatesInput(cv::vector<cv::Vec2f>& cueStick, cv::vector <cv::Vec2f>& cueBall, cv::vector<cv::Vec2f>& targetBalls){

	float bias = 360;
	cueStick[0][1] = bias - cueStick[0][1];
	cueStick[1][1] = bias - cueStick[1][1];
	cueBall[0][1] = bias - cueBall[0][1];
	for (int b = 0; b < targetBalls.size(); b++){
		targetBalls[b][1] = bias - targetBalls[b][1];
	}

}

void PhysicsModel::adjustCoordinatesOutput(cv::vector<Path>& trajectoryPoints){

	int bias = 360;
	for (int t = 0; t < trajectoryPoints.size(); t++){
		cv::Vec2f tempvec1 = trajectoryPoints[t].startPoint;
		cv::Vec2f tempvec2 = trajectoryPoints[t].endPoint;
		tempvec1[1] = bias - tempvec1[1];
		tempvec2[1] = bias - tempvec2[1];
		trajectoryPoints[t].startPoint = tempvec1;
		trajectoryPoints[t].endPoint = tempvec2;
	}

}
void PhysicsModel::swapCue(cv::vector<cv::Vec2f>& cueStick){
	
	cv::Vec2f placeholder = cueStick[0];
	cueStick[0] = cueStick[1];
	cueStick[1] = placeholder;

}

void PhysicsModel::calculateTrajectories(cv::vector<Path>& trajectoryPoints, cv::vector<cv::Vec2f> cueBall, cv::vector<cv::Vec2f> targetBalls, cv::vector<cv::Vec2f> cueStick){

	//Check if cue stick crosses cue ball
	cv::vector<cv::Vec2f> CueBallParams = getContactPointParams(0,cueStick[0],cueStick[1],cueBall,0,cueBallRadius);

	//Cue stick crosses cue ball and cue stick is close enough to cue ball
	if ((CueBallParams[0][0] == 0) && (norm(cueStick[1],cueBall[0]) < (float)maxCueDist)){
		trajectoryPoints = getTrajectoryGroup(CueBallParams[1], CueBallParams[4], targetBalls, 0);
	}

	return;

}

cv::vector<Path> PhysicsModel::getTrajectoryGroup(cv::Vec2f param1, cv::Vec2f param2, cv::vector<cv::Vec2f> targetBalls, int ballIndex)
{
	//Initialize variables
	Path mainTrajectory;
	Path branchTrajectory;
	cv::vector<Path> trajectoryPoints;
	cv::vector<cv::Vec2f> mainResults;
	cv::vector<cv::Vec2f> branchResults;
	cv::Vec2f ghostSlope;
	cv::vector<cv::Vec2f> ghostPoint1(1);
	cv::vector<cv::Vec2f> ghostPoint2(1);
	cv::vector<cv::Vec2f> ghostBackPoint(1);
	cv::vector<cv::Vec2f> ghostFrontPoint(1);
	int edgeCount = 0;
	int t = 1;
	//float maxBranchLength = cutFactor*(abs(pockets[0].y-pockets[3].y));
	int featureIndex = 0;
	int branchFlag;

	//Calculate start and end points of trajectories
	while ((t <= maxTotalTrajs)&&(edgeCount <= maxEdgeTrajs)){

		//Set start point
		if (featureIndex == 0){
			mainTrajectory.startPoint = param2;
		}
		else{
			mainTrajectory.startPoint = param1;
		}

		//Obtain parameters on next contacted feature in main trajectory group
		mainResults = getContactPointParams(featureIndex, param1, param2, targetBalls, ballIndex, 2*ballRadius);
		mainTrajectory.endPoint = mainResults[1];
		trajectoryPoints.push_back(mainTrajectory);

		//Establish param1 and param2 for the next loop. If ball was hit, calculate branch trajectories as well.
		if (mainResults[0][0] == 0){ //ball was hit
			
			//Calculate branch points
			branchTrajectory.startPoint = mainResults[1];
			if (mainResults[2][0] == mainResults[3][0]){ //infinity slope case
				ghostPoint1[0][0] = mainResults[1][0];
				ghostPoint1[0][1] = mainResults[1][1]-ballRadius;
				ghostPoint2[0][0] = mainResults[1][0];
				ghostPoint2[0][1] = mainResults[1][1]+ballRadius;
			}
			else{
				ghostSlope[0] = -1/((mainResults[3][1]-mainResults[2][1])/(mainResults[3][0]-mainResults[2][0]));
				if ((ghostSlope[0] < -100)||(ghostSlope[0] > 100)){
					ghostPoint1[0][0] = mainResults[1][0];
					ghostPoint1[0][1] = mainResults[1][1] - ballRadius;
					ghostPoint2[0][0] = mainResults[1][0];
					ghostPoint2[0][1] = mainResults[1][1] + ballRadius;
				}
				else{
					ghostPoint1 = backPointFromLine(1, mainResults[1], ghostSlope, mainResults[1], ballRadius);
					ghostPoint2[0] = frontFromBack(ghostPoint1[0], mainResults[1]);
				}		
			}
			if (norm(ghostPoint1[0], param1) < norm(ghostPoint2[0], param1)){
				ghostFrontPoint = ghostPoint2;
				ghostBackPoint = ghostPoint1;
				branchFlag = 1;
			}
			else if (norm(ghostPoint1[0],param1) > norm(ghostPoint2[0],param1)){
				ghostFrontPoint = ghostPoint1;
				ghostBackPoint = ghostPoint2;
				branchFlag = 1;
			}
			else{
				branchFlag = 0;
			}

			if (branchFlag == 1){
				branchResults = getContactPointParams(0,ghostBackPoint[0],ghostFrontPoint[0],targetBalls,ballIndex,2*ballRadius);
				branchTrajectory.endPoint = branchResults[1];
				trajectoryPoints.push_back(branchTrajectory);
			}
			
			//Next loop info for main trajectory
			featureIndex = 0;
			param1 = mainResults[2];
			param2 = mainResults[3];
			ballIndex = (int)mainResults[5][0];
			//edgeCount = 0 //not sure if will be useful

		}
		else if (mainResults[0][0] == 1){ //edge was hit
			featureIndex = 1;
			param1 = mainResults[1];
			param2 = mainResults[2];
			edgeCount++;
		}
		else { //pocket was hit
			t = maxTotalTrajs;
		}
		t++;
	}
	return trajectoryPoints;
}

cv::vector<cv::Vec2f> PhysicsModel::getContactPointParams(int featureIndex,cv::Vec2f param1,cv::Vec2f param2,cv::vector<cv::Vec2f> targetBalls,int targetBallIndex,float radiusBall)
{
	//Define empty vectors to store data, and determine number of target balls
	cv::vector<cv::Vec2f> nextParams;
	int B = targetBalls.size();
	cv::vector<cv::Vec2f> ballCandidates(B);
	cv::vector<float> candidateDists(B);
	int ballCandidateExists = 0; // assume there are no intersected balls prior to finding one

	//Find all intersections between line and target balls not including previously struck ball
	for (int b = 1; b <= B; b++){
		if (b != targetBallIndex){
			cv::vector<cv::Vec2f> candidate = backPointFromLine(featureIndex,param1,param2,targetBalls[b-1],radiusBall);
			if (candidate.size() == 1){
				ballCandidates[b-1] = candidate[0];
			}
		}
	}

	//Calculate distances from candidates to param1 (works both for input being ball or edge)
	for (int b = 1; b <= B; b++){
		if ((ballCandidates[b-1][0] == 0) && (ballCandidates[b-1][1] == 0)){
			candidateDists[b-1] = 0;
		}
		else{
			candidateDists[b-1] = norm(ballCandidates[b-1],param1);
		}
	}

	//Check if sufficient candidates exist, and find its index
	int i = 1;
	int ballMinIndex = 0;
	while ((ballCandidateExists == 0) && (i <= B)){
		if (candidateDists[i - 1] != 0){
			ballCandidateExists = 1;
			ballMinIndex = i;
		}
		i++;
	}

	//If sufficient ball candidates exist, find the sufficient one, and determine output parameters. 
	//Otherwise, determine output parameters for edge or pocket, if they are hit.
	if (ballCandidateExists == 1){
		float minDist = candidateDists[ballMinIndex-1];
		for (int i = 1; i <= B;i++){
			if ((candidateDists[i-1] < minDist)&&(candidateDists[i-1] != 0)){
				ballMinIndex = i;
			}
		}
		nextParams.push_back({ 0, 0 });
		cv::Vec2f backPoint2R = ballCandidates[ballMinIndex-1];
		nextParams.push_back(backPoint2R);
		cv::Vec2f backPointR = backFromBack(backPoint2R,targetBalls[ballMinIndex-1]);
		nextParams.push_back(backPointR);
		cv::Vec2f frontPointR = frontFromBack(backPointR,targetBalls[ballMinIndex-1]);
		nextParams.push_back(frontPointR);
		cv::Vec2f frontPoint2R = frontFromBack(backPoint2R,targetBalls[ballMinIndex-1]);
		nextParams.push_back(frontPoint2R);
		nextParams.push_back({(float)ballMinIndex,0});
	}	
	else{
		cv::vector<cv::Vec2f> pocketIntPoint = pocketPointFromLine(featureIndex,param1,param2,pocketRadius);
		if (pocketIntPoint.size()==0){ //edge hit
			nextParams.push_back({ 1, 0 });
			cv::vector<cv::Vec2f> edgeIntPoint = edgePointFromLine(featureIndex,param1,param2);
			nextParams.push_back(edgeIntPoint[0]);
			cv::Vec2f reflectSlope;
			if (featureIndex == 0){ //input was ball
				if (param1[0]==param2[0]){ //infinity slope case
					reflectSlope = { 0, 1 };
				}
				else{
					reflectSlope = { (-(param2[1]-param1[1])/(param2[0]-param1[0])), 0 };
				}
			}
			else{ //input was edge
				if (param2[1]==1){ //infinity slope case
					reflectSlope = { 0, 1 };
				}
				else{
					reflectSlope = { -param2[0], 0 };
				}
			}
			nextParams.push_back(reflectSlope);
		}
		else{ //pocket hit
			nextParams.push_back({ 2, 0 });
			nextParams.push_back(pocketIntPoint[0]);
		}
	}

	return nextParams;

}

cv::vector<cv::Vec2f> PhysicsModel::backPointFromLine(int featureIndex, cv::Vec2f param1, cv::Vec2f param2, cv::Vec2f target, float radius){

	//Calculate parameters of a line formed by param1 and param2, depending on input feature. Shorten variables for convenience
	float m;
	float b;
	float c;
	float d;
	int invert_flag = 0;

	if (featureIndex == 0){ //two x,y coords as input
		if (param1[0] == param2[0]){ //if slope of line is infinite, solve the inverted problem
			m = 0;
			b = param1[0];
			c = target[1];
			d = target[0];
			invert_flag = 1;
		}
		else{ //solve normal (non-inverted) problem
			m = (param2[1] - param1[1]) / (param2[0] - param1[0]);
			b = param1[1] - m*param1[0];
			c = target[0];
			d = target[1];
		}
	}
	else{ //x,y coord and slope as input
		if (param2[1]==1){ //if slope of line is infinite, solve the inverted problem
			m = 0;
			b = param1[0];
			c = target[1];
			d = target[0];
			invert_flag = 1;
		}
		else{ //solve normal (non-inverted) problem
			m = param2[0];
			b = param1[1] - m*param1[0];
			c = target[0];
			d = target[1];
		}
	}

	//Find discriminant to check if intersection exists
	float discrim = pow((2*(m*(b-d)-c)),2)-4*(pow(m,2)+1)*(pow(b,2)+pow(c,2)+pow(d,2)-2*b*d-pow(radius,2));

	//Initialize solution variables
	float x_sol_1;
	float x_sol_2;
	cv::Vec2f sol_1;
	cv::Vec2f sol_2;
	cv::vector<cv::Vec2f> backPoint;

	//Determine solutions, if they exist. In the case of two solutions, find solution closer to input feature
	if(discrim == 0){
		x_sol_1 = (-2 * (m*(b - d) - c)) / (2 * (pow(m, 2) + 1));
		sol_1 = { x_sol_1 , m*x_sol_1 + b };
		backPoint.push_back(sol_1);
	}
	else if(discrim > 0){
		x_sol_1 = ((-2 * (m*(b - d) - c)) + sqrt(discrim)) / (2 * (pow(m, 2) + 1));
		x_sol_2 = ((-2 * (m*(b - d) - c)) - sqrt(discrim)) / (2 * (pow(m, 2) + 1));
		sol_1 = { x_sol_1, m*x_sol_1 + b };
		sol_2 = { x_sol_2, m*x_sol_2 + b };
		if (norm(sol_1,param1) < norm(sol_2,param1)){
			backPoint.push_back(sol_1);
		}
		else{
			backPoint.push_back(sol_2);
		}
		//if input feature is two x,y coords, eliminate back point if it is not properly oriented
		if ((featureIndex == 0)&&(norm(backPoint[0],param1)<norm(backPoint[0],param2))){
			backPoint.clear();
		}
	}

	//Swap values if original problem was inverted
	if ((invert_flag == 1)&&(backPoint.size() == 1)){
		float placeHolder = backPoint[0][0];
		backPoint[0][0] = backPoint[0][1];
		backPoint[0][1] = placeHolder;
	}
	
	return backPoint;

}

cv::vector<cv::Vec2f> PhysicsModel::edgePointFromLine(int featureIndex, cv::Vec2f param1, cv::Vec2f param2){
	
	cv::vector<cv::Vec2f> edgeIntPoint;
	cv::vector<cv::Vec2f> edgePoint1;
	cv::vector<cv::Vec2f> edgePoint2;
	float slope;
	float yint;

	if (featureIndex == 0){ //input param is ball
		if (param1[0]==param2[0]){ //infinity slope case
			edgePoint1.push_back({param1[0],PT_UL[1]});
			edgePoint2.push_back({param1[0],PT_BL[1]});
		}
		else if (param1[1]==param2[1]){
			edgePoint1.push_back({PT_BL[0],param1[1]});
			edgePoint2.push_back({PT_BR[0],param1[1]});
		}
		else{
			slope = (param1[1]-param2[1]) / (param1[0]-param2[0]);
			yint = param1[1] - slope*param1[0];
		}
	}
	else{ //input param is edge
		if (param2[1] == 1){ //infinity slope case
			edgePoint1.push_back({param1[0],PT_UL[1]});
			edgePoint2.push_back({param1[0],PT_BL[1]});
		}
		else if (param2[0] == 0){
			edgePoint1.push_back({PT_BL[0], param1[1]});
			edgePoint2.push_back({PT_BR[0], param1[1]});
		}
		else{
			slope = param2[0];
			yint = param1[1] - slope*param1[0];
		}
	}

	cv::vector<cv::Vec2f> candidates(4);
	cv::vector<cv::Vec2f> candidates2;
	cv::vector<cv::Vec2f> candidates3;
	if ((edgePoint1.size()==0)&&(edgePoint2.size()==0)){

		candidates[0] = { PT_UR[0], slope*PT_UR[0] + yint };
		candidates[1] = { (PT_UL[1]-yint)/slope, PT_UL[1] };
		candidates[2] = { PT_UL[0], slope*PT_UL[0] + yint };
		candidates[3] = { (PT_BR[1]-yint)/slope, PT_BR[1] };

		int i;
		for (i = 0; i < 4; i++){
			if ((candidates[i][0]<=PT_UR[0])&&(candidates[i][1]<=PT_UL[1])&&(candidates[i][0]>=PT_UL[0])&&(candidates[i][1]>=PT_BR[1])){
				candidates2.push_back(candidates[i]);
			}
		}

		for (i = 0; i < candidates2.size(); i++){
			if (((candidates2[i][0] != PT_UR[0]) && (candidates2[i][1] != PT_UR[1])) || ((candidates2[i][0] != PT_UL[0]) && (candidates2[i][1] != PT_UL[1])) || ((candidates2[i][0] != PT_BL[0]) && (candidates2[i][1] != PT_BL[1])) || ((candidates2[i][0] != PT_BR[0]) && (candidates2[i][1] != PT_BR[1]))){
				candidates3.push_back(candidates2[i]);
			}
		}

		if (candidates3.size() == 1){
			edgeIntPoint = candidates3;
		}
		else{
			edgePoint1.push_back(candidates3[0]);
			edgePoint2.push_back(candidates3[1]);
		}

	}

	if (edgeIntPoint.size() == 0){
		if (featureIndex==0){
			if (norm(edgePoint1[0],param1) < norm(edgePoint1[0],param2)){
				edgeIntPoint = edgePoint2;
			}
			else{
				edgeIntPoint = edgePoint1;
			}
		}
		else{
			if (norm(edgePoint1[0], param1)<norm(edgePoint2[0], param1)){
			edgeIntPoint = edgePoint2;
		}
		else{
			edgeIntPoint = edgePoint1;
		}
	}
	}

	return edgeIntPoint;

}

cv::vector<cv::Vec2f> PhysicsModel::pocketPointFromLine(int featureIndex, cv::Vec2f param1, cv::Vec2f param2, float radius){
	
	cv::vector<cv::Vec2f> pocketCoordinates(4);
	pocketCoordinates[0] = PT_UR;
	pocketCoordinates[1] = PT_UL;
	pocketCoordinates[2] = PT_BL;
	pocketCoordinates[3] = PT_BR;

	cv::vector<cv::Vec2f> pocketIntPoint;

	int i;
	cv::vector<cv::Vec2f> candidate;
	for (i = 0; i < 4; i++){
		candidate = backPointFromLine(featureIndex,param1,param2,pocketCoordinates[i],radius);
		if (candidate.size() != 0){
			pocketIntPoint = candidate;
		}
	}
	return pocketIntPoint;
}

cv::Vec2f PhysicsModel::frontFromBack(cv::Vec2f backPoint, cv::Vec2f ballCoord){
	
	float DeltaX = ballCoord[0] - backPoint[0];
	float DeltaY = ballCoord[1] - backPoint[1];
	cv::Vec2f Delta = { DeltaX, DeltaY };
	cv::Vec2f newFrontPoint = ballCoord + Delta;
	return newFrontPoint;

}

cv::Vec2f PhysicsModel::backFromBack(cv::Vec2f backPoint, cv::Vec2f ballCoord){
	
	float DeltaX = ballCoord[0] - backPoint[0];
	float DeltaY = ballCoord[1] - backPoint[1];
	cv::Vec2f Delta = { DeltaX, DeltaY };
	cv::Vec2f newBackPoint = ballCoord - 0.5*Delta;
	return newBackPoint;

}

/*//TODO
cv::vector<cv::Vec2i> PhysicsModel::reducePoints(cv::Mat frame, cv::vector<cv::Vec2i> pockets)
{
	cv::vector<cv::Vec2i> fourPockets(4);
	fourPockets[0][0] = pockets[0][0];
	fourPockets[0][1] = pockets[0][1];
	fourPockets[1][0] = pockets[1][0];
	fourPockets[1][1] = pockets[1][1];
	fourPockets[2][0] = pockets[2][0];
	fourPockets[2][1] = pockets[2][1];
	fourPockets[3][0] = pockets[3][0];
	fourPockets[3][1] = pockets[3][1];
	return fourPockets;
}

void PhysicsModel::sortCorners(cv::vector<cv::Point2f>& corners, cv::Point2f center)
{
	cv::vector<cv::Point2f> top, bot;

	for (int i = 0; i < corners.size(); i++)
	{
		if (corners[i].y < center.y)
			top.push_back(corners[i]);
		else
			bot.push_back(corners[i]);
	}

	cv::Point2f tl = top[0].x > top[1].x ? top[1] : top[0];
	cv::Point2f tr = top[0].x > top[1].x ? top[0] : top[1];
	cv::Point2f bl = bot[0].x > bot[1].x ? bot[1] : bot[0];
	cv::Point2f br = bot[0].x > bot[1].x ? bot[0] : bot[1];

	corners.clear();
	corners.push_back(tl);
	corners.push_back(tr);
	corners.push_back(bl);
	corners.push_back(br);
}*/