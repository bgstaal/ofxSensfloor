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
: _highlightColor(255, 0, 0, 255), _numCycles(0), threshold(0.145)
{
	_font.loadFont("Courier New", 40);
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
		const int sleepTime = 5;

		_readSensorData();
		
		if (_numCycles * sleepTime % 10 == 0) // every second
		{
			_checkTimeout();
		}

		_numCycles++;
		sleep(sleepTime);
	}	
}

void ofxSensfloor::_checkTimeout()
{
	//cout << "CHECK TIME OUT!" << endl;
	float time = ofGetElapsedTimef();

	for (vector<TilePtr>::iterator t = _tiles.begin(); t != _tiles.end(); t++)
	{
		TilePtr tile = *t;

		float timeSinceLastUpdate = time - tile->latestUpdateTime;
		if (tile->latestUpdateTime > 0.0f && timeSinceLastUpdate > .1f) // 100ms
		{
			bool valuesOverThreshold = false;

			for (vector<Field>::iterator f = tile->fields.begin(); f != tile->fields.end(); f++)
			{
				if (f->val > threshold)
				{
					valuesOverThreshold = true;
					break;
				}
			}

			if (valuesOverThreshold)
			{
				_sendPollMessage(tile->tileID1, tile->tileID2);
			}
		}
		//cout << dec << "Tile " << (int)t->tileID1 << ", " << (int)t->tileID2 << " updated last :" <<  t->latestUpdateTime << endl;
	}
}

void ofxSensfloor::_sendPollMessage(unsigned char tileID1, unsigned char tileID2)
{
	vector<unsigned char> m;

	m.push_back(FRAME_START_IDENTIFIER);
	m.push_back(_roomID1); // GID
	m.push_back(_roomID2); // GID

	m.push_back(tileID1); // MODID
	m.push_back(tileID2); // MODID

	m.push_back(0x00); // SENS irrelevant for this message
	m.push_back(0x00); // FTID irrelevant for this message

	unsigned char meaning = 0x00;
	meaning |= 0x04; // status request

	m.push_back(meaning); // DEF

	m.push_back(0x00); // PARA paramater for wich to request status (0x00 is capacative value)

	m.push_back(0x00); // fill
	m.push_back(0x00); // fill
	m.push_back(0x00); // fill
	m.push_back(0x00); // fill
	m.push_back(0x00); // fill
	m.push_back(0x00); // fill
	m.push_back(0x00); // fill
	m.push_back(0x00); // fill

	_serial.writeBytes(&m[0], 17);
	_serial.flush(false, true);

	cout << "Poll message sent to Tile " << (int)tileID1 << ", " << tileID2 << endl;
}

void ofxSensfloor::_readSensorData()
{
	int numBytesAvailable = _serial.available();
	if (numBytesAvailable)
	{
		vector<unsigned char> buffer(numBytesAvailable);
		_serial.readBytes(&buffer[0], numBytesAvailable);

		//cout << endl;
		//cout << dec << "read " << numBytesAvailable << " from buffer" << endl;

		for (int i = 0; i < numBytesAvailable; i++)
		{
			unsigned char b = buffer[i];
			_latestMessage.push_back(b);

			//validate message
			if (_latestMessage.size() == 17 && _latestMessage[0] == FRAME_START_IDENTIFIER)
			{
				_parseMessage(_latestMessage);
			}

			// clear the last message on frame start identifyer (if the last message is complete)
			if (b == FRAME_START_IDENTIFIER && _latestMessage.size() >= 17)
			{
				_latestMessage.clear();
				_latestMessage.push_back(b);
			}

			//cout << hex << (int)b << " ";
		}

		_serial.flush(true, false);
		//cout << endl;
	}

	//cout << dec << "latest message size: " << _latestMessage.size() << endl;
}

void ofxSensfloor::_parseMessage(vector<unsigned char> m)
{
	unsigned char roomID1 = m[1];
	unsigned char roomID2 = m[2];

	//cout << endl << "PARSE MESSAGE" << endl;
	/*
	for (int i = 0; i < m.size(); i++)
	{
		cout << hex << (int)m[i] << " ";
	}
	*/

	//cout << endl;

	if (roomID1 == _roomID1 && roomID2 == _roomID2)
	{
		unsigned char tileID1 = m[3];
		unsigned char tileID2 = m[4];
		unsigned char meaning = m[7];

		auto t = _tileByIDs.find(make_pair(tileID1, tileID2));

		if (t != _tileByIDs.end())
		{
			t->second->latestUpdateTime = ofGetElapsedTimef();
			//cout << dec << "Tile id: " << (int)tileID1 << ", " << (int)tileID2 << endl; 
			//cout << dec << "IDs match a Tile: " << (int)t->second->tileID1 << ", " << (int)t->second->tileID2 << endl;

			int j = 0;
			// capactive data starts at index 9
			for (int i = 9; i < 17; i++)
			{
				int value = (int)m[i] - 0x80;
				float val = value / (float)0x80;
				t->second->fields[j].val = val;
				t->second->hasActiveField = val >= threshold;
				j++;
			}

			_updateActivePolygons();
		}
		else
		{
			ofLog(OF_LOG_WARNING) << "Received message from unidentified tile with id:" << tileID1 << ", " << tileID2 << endl;
		}
	}
	else
	{
		ofLog() << hex << "data is from transciever with room id: " << (int)roomID1 << ", " << (int)roomID2 << endl;
	}
}

void ofxSensfloor::_updateActivePolygons()
{
	_activeFields.clear();
	_activeFieldsNotClustered.clear();
	_clusters.clear();

	// get active fields
	for (vector<TilePtr>::iterator t = _tiles.begin(); t != _tiles.end(); t++)
	{
		TilePtr tile = *t;

		// implement when not testing
		/*
		if (tile->hasActiveField)
		{*/
			for (vector<Field>::iterator f = tile->fields.begin(); f != tile->fields.end(); f++)
			{
				if (f->val > threshold)
				{
					_activeFields.push_back(*f);
					_activeFieldsNotClustered.push_back(*f);
				}
			}
		//}
	}

	while (_activeFieldsNotClustered.size())
	{
		vector<Field> cluster;

		Field &f =_activeFieldsNotClustered[0];
		vector<Field> neighbours = _findNeighbouringFields(f);
		cluster.push_back(f);
		_clusters.push_back(cluster);

		_activeFieldsNotClustered.pop_front();
	}

	//cout << "num active fields: " << _activeFields.size() << endl;
}

vector<ofxSensfloor::Field> ofxSensfloor::_findNeighbouringFields(Field &field)
{
	vector<Field> fields;



	return fields;
}

ofMatrix4x4 ofxSensfloor::getTransform()
{
	return _transform;
}

void ofxSensfloor::setTransform(const ofMatrix4x4 &t)
{
	_transform = t;
	_updateTransform();
}

void ofxSensfloor::_updateTransform()
{
	_verticesTransformed.clear();

	for (int i = 0; i < _vertices.size(); i++)
	{
		_verticesTransformed.push_back(_vertices[i] * _transform);
	}

	_mesh.clearVertices();
	_mesh.addVertices(_verticesTransformed);
}

void ofxSensfloor::setup(unsigned char roomID1, unsigned char roomID2, int numCols, int numRows, vector<int> customTileIDs, ofVec2f tileSize)
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

			
			int tileID1 = useCustomLayout ? customTileIDs[IDIndex] : row;
			int tileID2 = useCustomLayout ? customTileIDs[IDIndex + 1] : col;

			if (tileID1 > -1 && tileID2 > -1)
			{	

				/*
				 7-6-5
				 |\|/|
				 8 0 4
				 |/|\|
				 1-2-3
				 */

				// enumeration starts with the triangle in the top rightmost corner
				_mesh.addTriangle(i0, i6, i5);
				_mesh.addTriangle(i0, i5, i4);
				_mesh.addTriangle(i0, i4, i3);
				_mesh.addTriangle(i0, i3, i2);
				_mesh.addTriangle(i0, i2, i1);
				_mesh.addTriangle(i0, i1, i8);
				_mesh.addTriangle(i0, i8, i7);
				_mesh.addTriangle(i0, i7, i6);
				
				TilePtr t(new Tile);
				t->tileID1 = (char)tileID1;
				t->tileID2 = (char)tileID2;
				t->latestUpdateTime = 0.0f;
			
				// enumeration starts with the triangle in the top rightmost corner
				t->fields.push_back(Field(i0, i6, i5));
				t->fields.push_back(Field(i0, i5, i4));
				t->fields.push_back(Field(i0, i4, i3));
				t->fields.push_back(Field(i0, i3, i2));
				t->fields.push_back(Field(i0, i2, i1));
				t->fields.push_back(Field(i0, i1, i8));
				t->fields.push_back(Field(i0, i8, i7));
				t->fields.push_back(Field(i0, i7, i6));
			
				_tiles.push_back(t);

				if (_tileByIDs.find(make_pair(tileID1, tileID2)) != _tileByIDs.end())
				{
					ofLog(OF_LOG_ERROR) << "Looks like you have duplicate tile ID's of " << (int)tileID1 << ", " << (int)tileID2 << endl;
				}
				else
				{
					_tileByIDs[make_pair(tileID1, tileID2)] = t;
				}
			}

			IDIndex += 2;
		}
	}
	
	_updateTransform();
}

void ofxSensfloor::setHighlightColor(const ofColor &c)
{
	_highlightColor = c;
} 

void ofxSensfloor::draw(bool drawIDs)
{
	//TODO: remove this call when not developing this functionality anymore
	_updateActivePolygons();

	ofPushStyle();
	ofSetLineWidth(1.0f);
	glPointSize(10);
	
	_mesh.drawWireframe();
	
	for (vector<TilePtr>::iterator t = _tiles.begin(); t != _tiles.end(); t++)
	{
		TilePtr tile = *t;

		for (int i = 0; i < tile->fields.size(); i++)
		{
			float val = tile->fields[i].val;

			if (val > threshold)
			{
				//ofSetColor(_highlightColor);
			}
			else
			{
				ofSetColor(255, 255, 255, 255 * val);
			}
			
			ofFill();
			ofVec3f &v0 = _verticesTransformed[tile->fields[i].index0];
			ofVec3f &v1 = _verticesTransformed[tile->fields[i].index1];
			ofVec3f &v2 = _verticesTransformed[tile->fields[i].index2];

			ofTriangle(v0, v1, v2);
			ofVec3f offset = ofVec3f(0, 0, 40 * val);
			//ofTriangle(v0+offset, v1+offset, v2+offset);
		}

		ofNoFill();
		ofSetLineWidth(3.0f);

		for (int i = 0; i < _clusters.size(); i++)
		{
			ofFloatColor f;
			f.setBrightness(1.0f);
			f.setSaturation(1.0f);
			f.setHue(abs(sin(i*10)));

			ofSetColor(f);
			for (int j = 0; j < _clusters[i].size(); j++)
			{
				Field &f = _clusters[i][j];
				ofTriangle(_verticesTransformed[f.index0], _verticesTransformed[f.index1], _verticesTransformed[f.index2]);
			}
		}
		
		
		ofSetColor(255, 255, 255);
		ofVec3f c = _verticesTransformed[tile->fields[0].index0];
		//ofDrawBitmapString(ss.str(), c);

		if (drawIDs)
		{
			stringstream ss;
			ss << (int)tile->tileID1 << "," << (int)tile->tileID2 << endl;

			ofPushMatrix();
			ofTranslate(c);
			ofScale(.25f, .25f);
				_font.drawString(ss.str(), 0, 0);
			ofPopMatrix();
		}	
	}

	ofPopStyle();
}