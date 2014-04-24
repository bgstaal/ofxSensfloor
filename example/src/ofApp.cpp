#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup()
{
	ofSetFrameRate(100);
	ofEnableAntiAliasing();
	ofEnableAlphaBlending();
	//ofEnableDepthTest();
	
	_light.setPointLight();
	_light.setPosition(200, 100, -200);
	
	_cam.setAutoDistance(false);
	_cam.setPosition(0.0f, 0.0f, 500.0f);
	_cam.setTarget(ofVec3f(0.0f, 0.0f, 0.0f));
	
	_sensFloor.setup(1, 1, 10, 10);
	//_sensFloor.addTile(0, 0, ofVec3f());
	//_sensFloor.addTile(1, 0, ofVec3f(25, 0));
	//_sensFloor.addTile(0, 1, ofVec3f(0, 25));
	
	//_doDrawDebugStuff = true;
}

void ofApp::update()
{
	
}

void ofApp::draw()
{
	ofBackground(0, 0, 0);
	
	_cam.begin();
		_light.enable();
	
		ofSetColor(255, 255, 255, 255);
		//ofDrawBox(0, 0, 0, 100, 100, 100);
	
		_light.disable();
		ofDisableLighting();
		_sensFloor.draw();
	
		_drawDebugStuff();
	
	_cam.end();
	
	ofPushMatrix();
		ofTranslate(ofGetWidth()*.5f, ofGetHeight()*.5f);
			_sensFloor.draw();
	ofPopMatrix();
}

void ofApp::_drawDebugStuff()
{
	if (_doDrawDebugStuff)
	{
		ofDrawGrid(500.0f, 8.0f, true, true, true, true);
	}
}

void ofApp::keyPressed(int key)
{
	ofLog() << "key pressed: " << key << endl;
	
	if (key == 102) // f
	{
		ofToggleFullscreen();
	}
	else if (key == 32) // space
	{
		
	}
}

void ofApp::keyReleased(int key)
{

}

void ofApp::mouseMoved(int x, int y)
{

}

void ofApp::mouseDragged(int x, int y, int button)
{
	
}

void ofApp::mousePressed(int x, int y, int button)
{

}

void ofApp::mouseReleased(int x, int y, int button)
{

}

void ofApp::windowResized(int w, int h)
{
	
}

void ofApp::gotMessage(ofMessage msg)
{

}

void ofApp::dragEvent(ofDragInfo dragInfo)
{

}