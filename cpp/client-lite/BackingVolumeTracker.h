#ifndef __BACKING_VOLUME_TRACKER_H__
#define __BACKING_VOLUME_TRACKER_H__

#include "VolumeTracker.h"

class BackingVolumeTracker : public VolumeTracker,  public Configurable {

    private:
        std::string file, backext;
        int writeival;
        int lastmod;

    public:

        BackingVolumeTracker ( DataManager &dm_ ) : VolumeTracker (dm_) {
            lastmod = 0;
            defOption("volume-file", &file, "Backing store for volume file.", "volume_backing_store.txt");
            defOption("backup-ext", &backext, "extension for writing", ".temp");
            defOption("vol-write-interval", &writeival, "Write volume (0 for read only)", 0);
        }

        bool initialize ( );
        void update ( const DataUpdate &du );
        bool write ( );
        ~BackingVolumeTracker ( ) { write(); }
};

#endif
