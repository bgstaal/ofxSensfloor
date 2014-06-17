#include "ofRectangle.h"
#include "ofThread.h"
#include "of3dGraphics.h"
#include "ofSerial.h"
#include <utility>
#include <map>
#include "ofTrueTypeFont.h"

class ofxSensfloor : public ofThread
{
	public:

		static const ofVec2f TILE_SIZE_SMALL;
		static const ofVec2f TILE_SIZE_LARGE;
		static const int BAUD_RATE_DEFAULT;

		float threshold;
	
		ofxSensfloor();
		~ofxSensfloor();

		void setup(unsigned char roomID1, unsigned char roomID2, int numCols, int numRows, vector<int> customTileIDs = vector<int>(), ofVec2f tileSize = TILE_SIZE_SMALL);
		void start(string portName, int baudRate = BAUD_RATE_DEFAULT);
		void start(int deviceNumber, int baudRate = BAUD_RATE_DEFAULT);
		void stop();
		//void addTile(unsigned char tileID1, unsigned char tileID2, ofVec3f pos);
	
		void draw(bool drawIDs = false);
		void setTransform(const ofMatrix4x4 &t);
		ofMatrix4x4 getTransform();
		
	private:
	
		struct Field
		{
			Field(int index0, int index1, int index2, unsigned char val = 0) : index0(index0), index1(index1), index2(index2), val() {};
			int index0, index1, index2;
			float val;
		};
		
		struct Tile
		{
			char tileID1, tileID2;
			vector<Field> fields;
			float latestUpdateTime;
		};

		typedef ofPtr<Tile> TilePtr;
	
		unsigned int _numCycles;
		char _roomID1, _roomID2;
		ofSerial _serial;
		vector<ofVec3f> _vertices;
		vector<ofVec3f> _verticesTransformed;
		vector<TilePtr> _tiles;
		map<pair<unsigned char, unsigned char>, TilePtr> _tileByIDs;
		ofVboMesh _mesh;
		vector<unsigned char> _latestMessage;
		ofTrueTypeFont _font;
		ofMatrix4x4 _transform;

		void threadedFunction();
		void _readSensorData();
		void _parseMessage(vector<unsigned char> msg);
		void _sendMessage(vector<unsigned char> msg);
		void _sendPollMessage(unsigned char tileID1, unsigned char tileID2);
		void _checkTimeout();
		void _updateTransform();
};