# RT11FS

RT11FS is a FUSE file system driver that understands the [RT-11](https://en.wikipedia.org/wiki/RT-11) file system. It 
implements a full read/write file system that can mount any RT-11 disk pack image. Currently, the driver has only been
tested on OS/X.

Once built, file systems can be mounted using the rt11fs binary:

`rt11fs mnt -i foo.dsk`

umount can be used to unmount the filesystem. 

## TODO/known issues
* Install rt11fs as a real OS X filesystem so it can be used with `mount'.
* Support compiling on Linux.
* Support squeeze. The file system keeps all files contiguous on disk (per DEC employees, this was done to make reads and
writes as fast as possibly since RT-11 is a real-time OS.) This leads to volume fragmentation, and the disk must occasionally 
be "squeezed", which moves all the files around to maximize runs of free space. RT-11 has a utility to do this, but it would
be nice to have the local file system support it.
* Support for unlinking open files. The *nix convention is that unlink() on an open handle doesn't delete the file until 
the last handle is closed. FUSE does some magic to make this work that is incompatible with RT-11 filenames. This
shouldn't be an issue for the standard use case of just exporting or editing files on the volume.
* Auto-detect text files and clean up trailing nul's. The RT-11 file system does not measure file size in bytes, but
512-byte blocks. If a file length doesn't work out to an intgral number of blocks, the file system pads the last
block will nul bytes. Editing the file and saving it again without trimming the nuls can push the nul padding into a new
block, making the file grow more than it needs with each successive edit. This could be fixed by detecting a file as
ASCII text and cleaning up the extraneous nuls when it is closed.

