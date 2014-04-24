#pragma once

#include "ofMain.h"
#include "ofxSensfloor.h"

class ofApp : public ofBaseApp{
	public:
		void setup();
		void update();
		void draw();
		
		void keyPressed(int key);
		void keyReleased(int key);
		void mouseMoved(int x, int y);
		void mouseDragged(int x, int y, int button);
		void mousePressed(int x, int y, int button);
		void mouseReleased(int x, int y, int button);
		void windowResized(int w, int h);
		void dragEvent(ofDragInfo dragInfo);
		void gotMessage(ofMessage msg);
	
	private:
		bool _doDrawDebugStuff;
		ofEasyCam _cam;
		ofLight _light;
		ofxSensfloor _sensFloor;
	
		void _drawDebugStuff();
};
