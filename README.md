# MP3Buster
Simple mp3 file analyzer -


# About
I listen to mp3 files. Some files have suffer from bit-rot or something else that causes disruption when playing back the music. After reading up on MPEG file format ( and id3v2 ) i come to the conclusion that missing MPEG header in file makes this sound. A MPEG file is built up by frames, each frame have a header and compressed audio data.
At least this is what i think.

This was the main goal, to find out if a mp3 file has this error makeing it 'scratch' at playback. But i thought it would be neat if it also could parse TAGs and maybe remove empty tags, add album art, rename files after a given pattern etc. And this is something i want to add when i find the time :)


# Credits
When researching this topic i came across others code wich helped me understand how things should be interpetaded and how much of information there realy is to get from an MP3 file, theres alot!
I wanted the ability to playback audio, even if it was not necessary for the project. There is serval open source projects that lets you do just that. But i came across Floris Creyf's work (https://github.com/FlorisCreyf/mp3-decoder) and it showed the process when it is not havely optimiezd. So the unpacking of the audio is from his work :)
Other valueble projects that helped me understand is
Taglib - https://taglib.org/
MPEG Audio Frame Header - Konrad Windszus article and code about the MPEG audio frame header.

# Fetures

# Compiling
you will need ..
cmake
alsa-lib-devel (optinal)
libasound2-dev (optinal)

git clone https://github.com/doyubkim/fluid-engine-dev.git
cd MP3Buster
mkdir build
cd build
cmake ..
make


# TODO
Make it cross-platform compatible
Check CRC for lame encoded files wich have theire own CRC.
Continue implementing decoding TAGs (id3v1, id3v2, Ape)
Implementing a config/script file interperar for maneging mp3 library (mass rename, strip/add TAGs etc.)
