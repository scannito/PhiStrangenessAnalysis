#ifndef EPOSEvent_h
#define EPOSEvent_h

#include <vector>

using namespace std;

class EPOSTrack {
public :

  double eta, y, pT;
  int pid, pity;
  bool charge, had;

};

class EPOSEvent {
public :
  
  vector<EPOSTrack> tracks;

  bool init() {
    tracks.clear();
    return true;
  }

};

#endif
