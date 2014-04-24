#include "ofxSensfloor.h"
#include "ofLog.h"

#define FRAME_START_IDENTIFIER 0xFD
#define MEANING_TRANSCIEVER_EVENT_FLAG 1

#define MEANING_SENSOR_MODULE_SIGNIFICANT_CHANGE_FLAG 1
#define MEANING_TRANSCIEVER_EVENT_ACKNOWLEDGEMENT 1
#define MEANING_MASTER_MESSAGE_TO_SYNCHRONIZE 1 << 1
#define MEANING_STATUS_REQUEST 1 << 2
#define MEANING_STATUS_ANSWER 1 << 3
#define MEANING_SENSOR_CAPICATANCE_VALUES_ARE_SIGNED 1 << 4
#define MEANING_CONFIGURATION_PARAMETERS 1 << 5
#define MEANING_RECALIBRATE_CAPACITANCE 1 << 6
#define MEANING_MSG_STEMS_FROM_TRANSCIEVER 1 << 7

const ofVec2f ofxSensfloor::TILE_SIZE_SMALL = ofVec2f(50, 50);
const ofVec2f ofxSensfloor::TILE_SIZE_LARGE = ofVec2f(50, 100);
const int ofxSensfloor::BAUD_RATE_DEFAULT = 115200;

ofxSensfloor::ofxSensfloor ()
{

}

ofxSensfloor::~ofxSensfloor()
{
	waitForThread(true);
}

void ofxSensfloor::start(int deviceNumber, int baudRate)
{
	_serial.listDevices();
	_serial.setup(deviceNumber, baudRate);
	startThread(true, true);
}

void ofxSensfloor::start(string portName, int baudRate)
{
	_serial.listDevices();
	_serial.setup(portName, baudRate);
	startThread(true, true);
}

void ofxSensfloor::stop()
{
	stopThread();
}

void ofxSensfloor::threadedFunction()
{
	while (isThreadRunning())
	{
		_readSensorData();
		sleep(5);
	}	
}

void ofxSensfloor::_readSensorData()
{
	int numBytesAvailable = _serial.available();
	if (numBytesAvailable)
	{
		vector<unsigned char> buffer(numBytesAvailable);
		_serial.readBytes(&buffer[0], numBytesAvailable);

		cout << endl;
		cout << dec << "read " << numBytesAvailable << " from buffer" << endl;

		for (int i = 0; i < numBytesAvailable; i++)
		{
			unsigned char b = buffer[i];
			_latestMessage.push_back(b);

			if (_latestMessage.size() == 17 && _latestMessage[0] == FRAME_START_IDENTIFIER)
			{
				_parseMessage(_latestMessage);
				_latestMessage.clear();
			}

			cout << hex << (int)b << " ";
		}

		cout << endl;
	}
}

void ofxSensfloor::_parseMessage(vector<unsigned char> m)
{
	unsigned char roomID1 = m[1];
	unsigned char roomID2 = m[2];

	if (roomID1 == _roomID1 && roomID2 == _roomID2)
	{
		ofLog() << "data is from this transciever" << endl;

		unsigned char tileID1 = m[3];
		unsigned char tileID2 = m[4];
		unsigned char meaning = m[7];

		if (meaning & MEANING_MSG_STEMS_FROM_TRANSCIEVER == 0) // msg is (probably )from sensor module
		{
			// capactive data starts at index 9

			auto t = _tileByIDs.find(make_pair(tileID1, tileID2));

			if (t != _tileByIDs.end())
			{
				for (int i = 9; i < 17; i++)
				{

				}
			}
			else
			{
				ofLog(OF_LOG_WARNING) << "Received message from unidentified tile with id:" << tileID1 << ", " << tileID2 << endl;
			}
		}
		else // msg is from transciever
		{

		}
	}
	else
	{
		ofLog() << hex << "data is from transciever with room id: " << (int)roomID1 << ", " << (int)roomID2 << endl;
	}
}

void ofxSensfloor::setup(unsigned char roomID1, unsigned char roomID2, int numRows, int numCols, vector<int> customTileIDs, ofVec2f tileSize)
{
	_roomID1 = roomID1;
	_roomID2 = roomID2;

	int numTiles = numRows * numCols;
	int numXVerts = (numCols * 2) + 1;
	int numYVerts = (numRows * 2) + 1;
	int numVerts = numXVerts * numYVerts;
	_vertices = vector<ofVec3f>(numVerts);
	
	float x = 0, y = 0;
	ofVec3f p0, p1, p2, p3, p4, p5, p6, p7, p8;
	int i0, i1, i2, i3, i4, i5, i6, i7, i8;
	ofVec2f s = tileSize*.5f;

	bool useCustomLayout = customTileIDs.size() > 0;
	int IDIndex = 0;

	if (useCustomLayout && customTileIDs.size() < numTiles * 2)
	{
		ofLog(OF_LOG_ERROR) << "not enough custom tileIDs to fill the entire sensfloor surface" << endl;
		return;
	}
	
	for (int row = 0; row < numRows; row++)
	{
		y = tileSize.y*row;
		
		for (int col = 0; col < numCols; col++)
		{
			x = tileSize.x * col;
			
			p0 = ofVec3f(x + s.x, y + s.y);
			p1 = ofVec3f(x, y);
			p2 = ofVec3f(x + s.x, y);
			p3 = ofVec3f(x + tileSize.x, y);
			p4 = ofVec3f(x + tileSize.x, y + s.y);
			p5 = ofVec3f(x + tileSize.x, y + tileSize.y);
			p6 = ofVec3f(x + s.x, y + tileSize.y);
			p7 = ofVec3f(x, y + tileSize.y);
			p8 = ofVec3f(x, y + s.y);
			
			int sRow = row * 2;
			int sCol = col * 2;
			i0 = ((sRow + 1) * numXVerts) + sCol + 1;
			i1 = (sRow * numXVerts) + sCol;
			i2 = (sRow * numXVerts) + sCol + 1;
			i3 = (sRow * numXVerts) + sCol + 2;
			i4 = ((sRow + 1) * numXVerts) + sCol + 2;
			i5 = ((sRow + 2) * numXVerts) + sCol + 2;
			i6 = ((sRow + 2) * numXVerts) + sCol + 1;
			i7 = ((sRow + 2) * numXVerts) + sCol;
			i8 = ((sRow + 1) * numXVerts) + sCol;
			
			_vertices[i0] = p0;
			_vertices[i1] = p1;
			_vertices[i2] = p2;
			_vertices[i3] = p3;
			_vertices[i4] = p4;
			_vertices[i5] = p5;
			_vertices[i6] = p6;
			_vertices[i7] = p7;
			_vertices[i8] = p8;

			/*
			 1-2-3
			 |\|/|
			 8 0 4
			 |/|\|
			 7-6-5
			 */
			int tileID1 = useCustomLayout ? customTileIDs[IDIndex] : row;
			int tileID2 = useCustomLayout ? customTileIDs[IDIndex + 1] : col;

			if (tileID1 > -1 && tileID2 > -1)
			{			
				// enumeration starts with the triangle in the top rightmost corner
				_mesh.addTriangle(i0, i3, i4);
				_mesh.addTriangle(i0, i4, i5);
				_mesh.addTriangle(i0, i5, i6);
				_mesh.addTriangle(i0, i6, i7);
				_mesh.addTriangle(i0, i7, i8);
				_mesh.addTriangle(i0, i8, i1);
				_mesh.addTriangle(i0, i1, i2);
				_mesh.addTriangle(i0, i2, i3);

				Tile t;
				t.tileID1 = tileID1;
				t.tileID2 = tileID2;
			
				// enumeration starts with the triangle in the top rightmost corner
				t.fields.push_back(Field(i0, i3, i4));
				t.fields.push_back(Field(i0, i4, i5));
				t.fields.push_back(Field(i0, i5, i6));
				t.fields.push_back(Field(i0, i6, i7));
				t.fields.push_back(Field(i0, i7, i8));
				t.fields.push_back(Field(i0, i8, i1));
				t.fields.push_back(Field(i0, i1, i2));
				t.fields.push_back(Field(i0, i2, i3));
			
				_tiles.push_back(t);

				if (_tileByIDs.find(make_pair(tileID1, tileID2)) != _tileByIDs.end())
				{
					ofLog(OF_LOG_ERROR) << "Looks like you have duplicate tile ID's of " << (int)tileID1 << ", " << (int)tileID2 << endl;
				}
				else
				{
					_tileByIDs[make_pair(tileID1, tileID2)] = &_tiles[_tiles.size()-1]; // pointer to last element in vector
				}
			}

			IDIndex += 2;
		}
	}
	
	_mesh.addVertices(_vertices);
}

void ofxSensfloor::draw()
{
	ofPushStyle();
	ofSetLineWidth(1.0f);
	glPointSize(10);
	
	_mesh.drawWireframe();

	//_mesh.drawVertices();
	
	for (vector<Tile>::iterator t = _tiles.begin(); t != _tiles.end(); t++)
	{
		for (int i = 0; i < t->fields.size(); i++)
		{
			float val = t->fields[i].val;
			ofSetColor(255, 255, 255, 255 * val);
			
			if (val > .9f) ofSetColor(0, 255, 255, 255 * val);
			
			ofFill();
			ofVec3f &v0 = _vertices[t->fields[i].index0];
			ofVec3f &v1 = _vertices[t->fields[i].index1];
			ofVec3f &v2 = _vertices[t->fields[i].index2];
			ofTriangle(v0, v1, v2);
		}
		
		stringstream ss;
		ss << (int)t->tileID1 << "," << (int)t->tileID2 << endl;
		
		ofSetColor(255, 255, 255);
		ofVec3f c = _vertices[t->fields[0].index0];
		ofDrawBitmapString(ss.str(), c);
	}

	ofPopStyle();
}