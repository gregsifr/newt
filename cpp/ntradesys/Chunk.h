
#ifndef __CHUNK_H__
#define __CHUNK_H__

class Chunk{
  int _chunkSz;
 public:
  Chunk(){
    _chunkSz=100; // default value
  }
  int getChunkSize() { return _chunkSz;}
  void setChunkSize( int sz ) { _chunkSz = sz; }
};


#endif   // __CHUNK_H__
