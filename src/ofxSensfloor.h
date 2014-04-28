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
	
		/*
		Configure the terminal such that it uses the new
		COM-port with 155kBaud and 8N1 transmission
		format. In case this is supported by the terminal,
		configure the message display such that a carriage
		return is added every 17 bytes as every
		message from the SE3-P is 17 bytes long.
		 */
		
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
		void addTile(unsigned char tileID1, unsigned char tileID2, ofVec3f pos);
	
		void draw();
		
	
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
		vector<TilePtr> _tiles;
		map<pair<unsigned char, unsigned char>, TilePtr> _tileByIDs;
		ofVboMesh _mesh;
		vector<unsigned char> _latestMessage;
		ofTrueTypeFont _font;

		void threadedFunction();
		void _readSensorData();
		void _parseMessage(vector<unsigned char> msg);
		void _sendMessage(vector<unsigned char> msg);
		void _sendPollMessage(unsigned char tileID1, unsigned char tileID2);
		void _checkTimeout();
};