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
: _highlightColor(255, 0, 0, 255), _numCycles(0), threshold(0.07), _isConnected(false)
{
	_font.loadFont("Courier New", 40);
}

ofxSensfloor::~ofxSensfloor()
{
	waitForThread(true);
}

void ofxSensfloor::start(int deviceNumber, int baudRate, int maxReconnects)
{
	_serial.listDevices();

	_numReconnects = 0;
	_connectByName = false;
	_deviceNumber = deviceNumber;
	_baudRate = baudRate;
	_maxReconnects = maxReconnects;

	startThread(true, true);
}

void ofxSensfloor::start(string portName, int baudRate, int maxReconnects)
{
	_serial.listDevices();

	_numReconnects = 0;
	_connectByName = true;
	_portName = portName;
	_baudRate = baudRate;
	_maxReconnects = maxReconnects;

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
		if (_isConnected)
		{
			const int sleepTime = 2;

			_readSensorData();
		
			if (_numCycles * sleepTime % 10 == 0)
			{
				_checkTimeout();
			}

			_numCycles++;
			sleep(sleepTime);
		}
		else if (_numReconnects < _maxReconnects)
		{
			if (_connectByName)
			{
				ofLog(OF_LOG_NOTICE) << "ofxSensfloor connecting to " << _portName << " (num tries: " << _numReconnects + 1 << ")" << endl;
				_isConnected = _serial.setup(_portName, _baudRate);
			}
			else
			{
				ofLog(OF_LOG_NOTICE) << "ofxSensfloor connecting to device number " << _deviceNumber << " (num tries: " << _numReconnects + 1 << ")" << endl;
				_isConnected = _serial.setup(_deviceNumber, _baudRate);
			}

			_numReconnects ++;
			sleep(100);
		}
		else
		{
			ofLog(OF_LOG_ERROR) << "ofxSensfloor failed to connect after " << _numReconnects << " tries" << endl;
			stopThread();
		}
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

			for (vector<FieldPtr>::iterator f = tile->fields.begin(); f != tile->fields.end(); f++)
			{
				if ((*f)->val > threshold)
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

	//cout << "Poll message sent to Tile " << (int)tileID1 << ", " << tileID2 << endl;
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
				t->second->fields[j]->val = val;
				if (val >= threshold) t->second->hasActiveField = true;
				j++;
			}

			_updateActivePolygons();
		}
		else
		{
			cout << hex << "Received message from unidentified tile with id: " << (int)tileID1 << ", " << (int)tileID2 << endl;
		}
	}
	else
	{
		ofLog() << hex << "data is from transciever with room id: " << (int)roomID1 << ", " << (int)roomID2 << endl;
	}
}

ofMatrix4x4 ofxSensfloor::getTransform()
{
	return _transform;
}

void ofxSensfloor::setTransformFromPosition(const ofVec3f &pos)
{
	ofMatrix4x4 t;
	t.translate(pos);

	setTransform(t);
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

void ofxSensfloor::setup(unsigned char roomID1, unsigned char roomID2)
{
	_roomID1 = roomID1;
	_roomID2 = roomID2;
	_updateTransform();
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
			int tileID1 = useCustomLayout ? customTileIDs[IDIndex] : row;
			int tileID2 = useCustomLayout ? customTileIDs[IDIndex + 1] : col;
			
			p0 = ofVec3f(x + s.x, y + s.y);
			p1 = ofVec3f(x, y);
			p2 = ofVec3f(x + s.x, y);
			p3 = ofVec3f(x + tileSize.x, y);
			p4 = ofVec3f(x + tileSize.x, y + s.y);
			p5 = ofVec3f(x + tileSize.x, y + tileSize.y);
			p6 = ofVec3f(x + s.x, y + tileSize.y);
			p7 = ofVec3f(x, y + tileSize.y);
			p8 = ofVec3f(x, y + s.y);

			_addTile(tileID1, tileID2, p0, p1, p2, p3, p4, p5, p6, p7, p8);
			
			IDIndex += 2;
		}
	}
	
	_updateTransform();
}

void ofxSensfloor::_addTile(int tileID1, int tileID2, ofVec3f p0, ofVec3f p1, ofVec3f p2, ofVec3f p3, ofVec3f p4, ofVec3f p5, ofVec3f p6, ofVec3f p7, ofVec3f p8)
{
	int i0 = _addVertexAndReturnIndex(p0);
	int i1 = _addVertexAndReturnIndex(p1);
	int i2 = _addVertexAndReturnIndex(p2);
	int i3 = _addVertexAndReturnIndex(p3);
	int i4 = _addVertexAndReturnIndex(p4);
	int i5 = _addVertexAndReturnIndex(p5);
	int i6 = _addVertexAndReturnIndex(p6);
	int i7 = _addVertexAndReturnIndex(p7);
	int i8 = _addVertexAndReturnIndex(p8);

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
		t->fields.push_back(FieldPtr(new Field(i0, i6, i5)));
		t->fields.push_back(FieldPtr(new Field(i0, i5, i4)));
		t->fields.push_back(FieldPtr(new Field(i0, i4, i3)));
		t->fields.push_back(FieldPtr(new Field(i0, i3, i2)));
		t->fields.push_back(FieldPtr(new Field(i0, i2, i1)));
		t->fields.push_back(FieldPtr(new Field(i0, i1, i8)));
		t->fields.push_back(FieldPtr(new Field(i0, i8, i7)));
		t->fields.push_back(FieldPtr(new Field(i0, i7, i6)));
			
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
}

int ofxSensfloor::_addVertexAndReturnIndex(ofVec3f p)
{
	//check for excisting verts at this position
	for (int i = 0; i < _vertices.size(); i++)
	{
		if (p == _vertices[i])
		{
			return i;
		}
	}

	_vertices.push_back(p);
	return _vertices.size() - 1;
}

void ofxSensfloor::addStrip(vector<int> IDs, ofVec3f pos, ofVec3f tileSize, float rotation)
{
	ofVec3f p0, p1, p2, p3, p4, p5, p6, p7, p8;
	ofVec2f s = tileSize*.5f;
	float x = 0, y = 0;
	int num =  IDs.size() / 2;
	int prRow = num;
	if (tileSize == TILE_SIZE_SMALL) prRow /= 2;

	for (int i = 0; i < num; i++)
	{
		/*
		7-6-5
		|\|/|
		8 0 4
		|/|\|
		1-2-3
		*/

		p0 = ofVec3f(x + s.x, y + s.y);
		p1 = ofVec3f(x, y);
		p2 = ofVec3f(x + s.x, y);
		p3 = ofVec3f(x + tileSize.x, y);
		p4 = ofVec3f(x + tileSize.x, y + s.y);
		p5 = ofVec3f(x + tileSize.x, y + tileSize.y);
		p6 = ofVec3f(x + s.x, y + tileSize.y);
		p7 = ofVec3f(x, y + tileSize.y);
		p8 = ofVec3f(x, y + s.y);		

		_addTile(IDs[i*2], IDs[(i*2)+1], p0, p1, p2, p3, p4, p5, p6, p7, p8);

		if (i > 0 && (i+1) % prRow == 0)
		{
			x = 0;
			y += tileSize.y;
		}
		else
		{
			x += tileSize.x;
		}
	}
}

void ofxSensfloor::setHighlightColor(const ofColor &c)
{
	_highlightColor = c;
} 

void ofxSensfloor::_updateActivePolygons()
{
	_activeFields.clear();
	_activeFieldsNotClustered.clear();
	_clusters.clear();
	_clusteredEdges.clear();

	// --------------------------------
	// get active fields
	// --------------------------------
	for (vector<TilePtr>::iterator t = _tiles.begin(); t != _tiles.end(); t++)
	{
		TilePtr tile = *t;

		if (tile->hasActiveField)
		{
			for (vector<FieldPtr>::iterator f = tile->fields.begin(); f != tile->fields.end(); f++)
			{
				if ((*f)->val > threshold)
				{
					_activeFields.push_back(*f);
					_activeFieldsNotClustered.push_back(*f);
				}
			}
		}
	}


	// --------------------------------
	// arrange active fields into clusters
	// --------------------------------
	while (_activeFieldsNotClustered.size())
	{
		vector<FieldPtr> cluster;

		FieldPtr f =_activeFieldsNotClustered[0];
		_activeFieldsNotClustered.pop_front();

		vector<FieldPtr> neighbours = _findNeighbouringFields(f);
		cluster.push_back(f);

		if (neighbours.size()) cluster.insert(cluster.end(), neighbours.begin(), neighbours.end());
		_clusters.push_back(cluster);
	}


	// --------------------------------
	// retrieve all outer edges for each cluster
	// --------------------------------
	for (vector<vector<FieldPtr> >::iterator c = _clusters.begin(); c != _clusters.end(); c++)
	{
		map<Edge, int> edgeRefCount;
		vector<Edge> edges;

		//loop through each field in cluster and retrieve all the edges + 
		for (int i = 0; i < c->size(); i++)
		{
			FieldPtr f = (*c)[i];

			Edge e0 = _edgeFromIndices(f->index0, f->index1);
			Edge e1 = _edgeFromIndices(f->index1, f->index2);
			Edge e2 = _edgeFromIndices(f->index2, f->index0);

			_addOrIncrementEdgeCount(e0, edgeRefCount);
			_addOrIncrementEdgeCount(e1, edgeRefCount);
			_addOrIncrementEdgeCount(e2, edgeRefCount);
		}

		// remove duplicate edges
		for (map<Edge, int>::iterator it = edgeRefCount.begin(); it != edgeRefCount.end(); it++)
		{
			if (it->second == 1)
			{
				edges.push_back(it->first);
			}
		}

		_clusteredEdges.push_back(edges);
	}

	// --------------------------------
	// order edges into polygons
	// --------------------------------

	vector<_Blob> blobs;

	for (int i = 0; i < _clusteredEdges.size(); i++)
	{
		_Blob b;
		b.fields = _clusters[i];

		vector<Edge> unvisitedEdges = _clusteredEdges[i];

		Edge firstEdge = unvisitedEdges[0];
		unvisitedEdges.erase(unvisitedEdges.begin());

		b.indices.push_back(firstEdge.first);
		b.indices.push_back(firstEdge.second);
		int currentIndex = firstEdge.second;
		
		int j = 0;

		while (j < unvisitedEdges.size())
		{
			j = 0;
			//cout << unvisitedEdges.size() << endl;

			for (vector<Edge>::iterator e = unvisitedEdges.begin(); e != unvisitedEdges.end(); e++)
			{
				bool edgeFound = false;
				//cout << "current index: " << currentIndex << endl;
				//cout << "first: " << e->first << " second: " << e->second << endl;

				if (currentIndex == e->first)
				{
					currentIndex = e->second;
					edgeFound = true;
				}
				else if (currentIndex == e->second)
				{
					currentIndex = e->first;
					edgeFound = true;
				}

				if (edgeFound)
				{
					b.indices.push_back(currentIndex);
					unvisitedEdges.erase(e);
					j--;
					break;
				}

				j++;
			}
		}

		b.indices.push_back(firstEdge.first);

		blobs.push_back(b);
	}

	lock();
		_blobs = blobs;
	unlock();
}

vector<ofxSensfloor::Blob> ofxSensfloor::getBlobs()
{
	vector<Blob> outBlobs;
	vector<_Blob> blobs = _getBlobs();

	for (int i = 0; i < blobs.size(); i++)
	{
		_Blob &b = blobs[i];
		Blob blob;

		for (int j = 0; j < b.indices.size(); j++ )
		{
			blob.verts.push_back(_verticesTransformed[blobs[i].indices[j]]);
		}

		outBlobs.push_back(blob);
	}

	return outBlobs;
}

void ofxSensfloor::_addOrIncrementEdgeCount(Edge &e, map<Edge, int> &targetMap)
{
	map<Edge, int>::iterator it = targetMap.find(e);

	if (it == targetMap.end())
	{
		targetMap[e] = 1;
	}
	else
	{
		targetMap[e] ++;
	}
}

ofxSensfloor::Edge ofxSensfloor::_edgeFromIndices(const int &index0, const int &index1)
{
	Edge e;

	// order edge indices from low to high
	if (index0 < index1)
	{
		e.first = index0;
		e.second = index1;
	}
	else
	{
		e.first = index1;
		e.second = index0;
	}

	return e;
}

vector<ofxSensfloor::FieldPtr> ofxSensfloor::_findNeighbouringFields(FieldPtr field)
{
	vector<FieldPtr> fields;

	int i = 0;

	while (i < _activeFieldsNotClustered.size())
	{
		FieldPtr f = _activeFieldsNotClustered[i];

		// count shared vertices
		int numSharedVerts = 0;
		if (field->index1 == f->index1 || field->index1 == f->index2) numSharedVerts++;
		if (field->index2 == f->index1 || field->index2 == f->index2) numSharedVerts++;
		if (field->index0 == f->index0) numSharedVerts++;

		// if it shares 2 or more verts it shares one edge and therefore is neighbours
		if (numSharedVerts >= 2)
		{
			fields.push_back(f); // add it to neighbouring fields
			_activeFieldsNotClustered.erase(_activeFieldsNotClustered.begin() + i); // remove it from active fields

			//find neighbours of that one as well to see if more are connected
			vector<FieldPtr> neighbours = _findNeighbouringFields(f);
			if (neighbours.size()) fields.insert(fields.end(), neighbours.begin(), neighbours.end());

			// do not increment as we just removed one element
		}
		else
		{
			i++;
		}
	}

	return fields;
}

vector<ofxSensfloor::_Blob> ofxSensfloor::_getBlobs()
{
	ofScopedLock lock(mutex);
		return _blobs;
}

void ofxSensfloor::draw(bool drawBlobs, bool drawIDs)
{
	ofPushStyle();
	ofSetLineWidth(1.0f);
	glPointSize(10);
	ofDisableDepthTest();
	
	ofSetColor(255, 255, 255, 30);
	_mesh.drawWireframe();
	
	for (vector<TilePtr>::iterator t = _tiles.begin(); t != _tiles.end(); t++)
	{
		TilePtr tile = *t;

		for (int i = 0; i < tile->fields.size(); i++)
		{
			float val = tile->fields[i]->val;

			if (val > threshold)
			{
				ofSetColor(_highlightColor);
			}
			else
			{
				ofSetColor(255, 255, 255, 255 * val);
			}
			
			ofFill();
			ofVec3f &v0 = _verticesTransformed[tile->fields[i]->index0];
			ofVec3f &v1 = _verticesTransformed[tile->fields[i]->index1];
			ofVec3f &v2 = _verticesTransformed[tile->fields[i]->index2];

			ofTriangle(v0, v1, v2);
			ofVec3f offset = ofVec3f(0, 0, 40 * val);
		}
		

		if (drawIDs)
		{
			ofSetColor(255, 255, 255);
			ofVec3f c = _verticesTransformed[tile->fields[0]->index0];
			stringstream ss;
			ss << (int)tile->tileID1 << "," << (int)tile->tileID2 << endl;

			ofPushMatrix();
			ofTranslate(c);
			ofScale(.25f, .25f);
				_font.drawString(ss.str(), 0, 0);
			ofPopMatrix();
		}	
	}

	if (drawBlobs)
	{
		vector<_Blob> blobs = _getBlobs();
		glLineWidth(3);
		
		for (int i  = 0; i < blobs.size(); i++)
		{
			ofFloatColor f;
			f.setBrightness(1.0f);
			f.setSaturation(.5f);
			f.setHue(fabs(sin(i*20)));

			ofSetColor(f);

			glBegin(GL_LINE_STRIP);

			_Blob &b = blobs[i];

			for (int j  = 0; j < b.indices.size(); j++)
			{
				glVertex2f(_verticesTransformed[b.indices[j]].x, _verticesTransformed[b.indices[j]].y);
			}

			glEnd();
		}
	}

	ofPopStyle();
}