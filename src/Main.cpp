#include <iostream>
#include <chrono> //milliseconds
#include <thread> //sleep_for
#include <getopt.h>
#include <filesystem> // C++17
#include <algorithm>
#include <map>

#include <alsa/asoundlib.h> /* dnf install alsa-lib-devel */ /* apt install libasound2-dev */
#include "mpeg/Mp3.hpp"
#include "logger.hpp"


// Set log level
// unsigned GlobalLogLevel = LOG_VERBOSE;
unsigned GlobalLogLevel = LOG_INFO;
// unsigned GlobalLogLevel = LOG_WARNING;
// unsigned GlobalLogLevel = LOG_ERROR;
// unsigned GlobalLogLevel = LOG_SILENT;


void version() {
  std::cout << "MP3Crawler 1.0" << std::endl;
  std::cout << "Copyright (C) 2022 Sebastian Ekman." << std::endl;
}

void usage()
{
    std::cout << "\nUsage:\tMP3Crawler [OPTIONS]...  FILE|PATH" << std::endl;
    std::cout << "Batchmode will only output one line per file with information" << std::endl;
    std::cout << "  -n,\t\t--nobatch\t\t\tStops batchmode for multiple files and will show full information" << std::endl;
    // std::cout << "  -c <path>,\t\t--config <path>\t\tpath to config file describeing action to be taken" << std::endl;
    std::cout << "  -e <path>,\t--erroroutput <path>\tPuts full path of the files giving errors into a file" << std::endl;
    std::cout << "  -l,\t\t--log <1-4>\t\tdetermines the log level (see log levels below) thats get printed" << std::endl;
    std::cout << "  -r,\t\t--recursive\t\tRecursivly going trought given path" << std::endl;
    std::cout << "  -p,\t\t--playback\t\tplays the audio from the mp3 file" << std::endl;
    std::cout << "  -d,\t\t--dontstop\t\tDont stop at first bad frame, cuntinue analyzeing." << std::endl;
    std::cout << "    \t\t--help\t\t\tprint this help screen" << std::endl;
    std::cout << "    \t\t--version\t\tprint version information\n" << std::endl;
    std::cout << "Loglevels:" << std::endl;
    std::cout << "\t1: Error" << std::endl;
    std::cout << "\t2: Errors and Warnings" << std::endl;
    std::cout << "\t3: Errors and Warnings and information (default)" << std::endl;
    std::cout << "\t4: Errors and Warnings and Information and Verbos" << std::endl;

    // TODO: Info about config file ??

}

/*
  (new) WORKFLOW: !=TODO *=DONE
    * Given one file, the program gives a summary (either normal summary or verbose summary).
    * Given two files, the program gives out a comparision between the two
    * Given more then two files or folders, the program gives ut bulk-mode information

  TODO:
   MAJOR:
    * Check content if this realy is a mp3 file. Now we only go by extension
    * Implement Config option
    * history, a temporary locale file thats keeps the hashes of the known good files. so when it runs again, it wont check thoes
      and a option for ingnoring history and re-scan
    * Profiler? to make it faster..

  MINOR:
    * Make Verbose logging somewhat useful (or get rid of it)
    * Tidy up File.hpp/cpp , implement and test write functions etc.
    * Tidy up argument/config hanteraren

    * remove stopbatchmode and have 'force-batchmode' and 'force-singlemode'




  NOT EVEN STARTED:
    * File & Tag manager:
      + Name convention for known bands. ie. I say that all ACDC songs shouled be spelled:
        'AC/DC'. Then the manager will 'convert' all verationts of ACDC to that spelling..
      + AudioRanger will complete missing information with data obtained from high quality
        online sources like the music databases MusicBrainz and AcoustID.
      + Add high quality album artwork
      + Use different name patterns for single artist albums, compilation albums and single tracks

  TEST:
    Går det att fixa dessa miss ljud genom att ta 'sy ihop' dom fungerande frames en?
    Alltså flytta nästa valid frame till den plats som den 'ska vara'

    alternativt att "fixa" den trasiga headern och hoppas på att ljud-datan är korrekt

  BUGG:
    + Vid Batchmode output så blir det något fel med std::setw eller std::left eller nått, för
     när det kommer en fil med ÅÄÖ i sig så blir det inte lika "brätt" till ':OK' outputen
     (se: https://stackoverflow.com/questions/22040052/stdsetw-considering-special-characters-as-two-characters)


  Fundering:

   Det verkar som att EEEK ljudet skapas av när mpeg headern inte är i sync, alltså på det stället det borde.
   Dock 2 fel syncade frames i närheten av varandra ger bara ett 'hick' i ljudet.
   Se Kalle Bah - 03 - You are my angel.mp3.
   Fast kan ge ett konstigt ljud också..!? se Beastie Boys - Fight for your right to party

   men fyra miss sync i närheten av varandra ger EEEK!, se Rem - Man on the moon.mp3
   Det finns 2 dåliga frames i Meli C - Nothen Star, när jag spelar så blir det ingen eek,
   men med ffplay så blir det eek ??

   1 Bad Frame , ger konstigt ljud i bland..
   Ella Fitz- Cheek to Cheek.mp3 (ffplay) <- inget konstigt ljud
   Eric Carmen - All By Myself.mp3 <- konstigt ljud

   Kolla på PCM på framen före, under och efter bad frame på båda dessa filer,
   så kanske man kan se en skillnad som kan berätta ifall det orsakar en EEK ?

   Xing visar antal frames som ex. 1031, men jag räknar till 1032. Är det för att xing
   uptar en tom mpeg frame och räknar inte med den? (testat på jamp3-test/mp3/vbr/abr032.mp3)

   Nu uppskattas längden på ljudet utifrån första mpeg frame (VBRI,Xing eller CBR)
   Och detta är bra vid "fast mode", men kanske korrekt läsa längden av ljudet när
   hela filen läses? räkna ihop alla frame size * bitrate??

*/

void AddToQue(std::filesystem::path path, myConfig& cfg, std::vector<std::filesystem::path>& queue) {

  std::string ext = path.extension().string();
  std::transform(ext.begin(), ext.end(),ext.begin(), ::toupper);

  if(ext == ".MP3"){
    debug(LOG_VERBOSE) << "Adding " << path << " to que" << std::endl;
    queue.push_back(path);

    if(path.filename().string().length() > cfg.longestWidthEntry)
      cfg.longestWidthEntry = path.filename().string().length();
  }
}

int HandleArgumentsRequest(int argc, char *argv[], myConfig& cfg, std::vector<std::filesystem::path>& queue ) {


  // argc (ARGument Count) is int and stores number of command-line arguments passed by
  // the user including the name of the program. So if we pass a value to a program,
  // value of argc would be 2 (one for argument and one for program name)

  // If we got no arguments we exit
  if(argc == 1){
    std::cout << "No arguments" << std::endl;
    return 0;
  }

  // The variable optind is the index of the next element to be processed in argv.
  // The system initializes this value to 1. The caller can reset it to 1 to restart
  // scanning of the same argv, or when scanning a new argument vector.


  // The struct option structure has these fields:

  // const char *name - This field is the name of the option. It is a string.
  // int has_arg      - This field says whether the option takes an argument. It is an integer, and there are three legitimate values:
  //                      no_argument,
  //                      required_argument,
  //                      optional_argument.
  // int *flag        - If flag is a null pointer, then the val is a value which identifies this option. Often these values are chosen to uniquely identify particular long options.
  //                    If flag is not a null pointer, it should be the address of an int variable which is the flag for this option.
  //
  // int val          The value in val is the value to store in the flag to indicate that the option was seen.

  static struct option long_options[] =
    {
      {"help",    no_argument, 0, 'h'}, // Print out usage
      {"version", no_argument, 0, 'V'}, // Print out version
      {"nobatch", no_argument, 0, 'n'},
      {"erroroutput",   required_argument, 0, 'e'},
      // {"config",   required_argument, 0, 'c'},
      {"log",   required_argument, 0, 'l'},
      {"recursive",   no_argument, 0, 'r'},
      {"playback",   no_argument, 0, 'p'},
      {"errorstop",   no_argument, 0, 'e'},
      {0, 0, 0, 0}
    };

  // Short options (note: No short options for version, help)
  const char* short_options="ncelprd";

  int c;
  int option_index = -1;


  // optind - The variable optind is the index of the next element to be processed in argv.
  //          The system initializes this value to 1. The caller can reset it to 1 to restart
  //          scanning of the same argv, or when scanning a new argument vector.
  // argc - arguments count
  // argv - argument array
  while ((c = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1)
  {

    switch (c)
    {
        case 'h': { usage(); break; }
        case 'V': { version(); break; }
        case 'n': { cfg.stopbatchmode = true; break; }
        // case 'c': { std::cout << "config option!" << std::endl; break; }
        case 'e': {

          // So if we useing long option, the value is in optarg
          // else we need to fetch next char
          if(option_index >= 0)
            cfg.erroroutputpath = optarg;
          else
            cfg.erroroutputpath = argv[optind];
            optind++; // ..and move index forward

          if(cfg.erroroutputpath != "") {
            cfg.erroroutput = true;
          }
          break;
        }

        case 'l': {
          int value;

          if(option_index >= 0) {
            value = strtol(optarg, NULL, 10);
          } else {
            value = strtol(argv[optind], NULL, 10);
            optind++; // ..and move index forward
          }

          if(value >= 0 && value <= 6) {
            cfg.loglevel = value;
          } else {
            debug(LOG_WARNING) << "Invalid log level given, going with default log level " << cfg.loglevel << std::endl;
          }
          break;
        }

        case 'p': { cfg.playback = true; break; }
        case 'r': { cfg.recursive = true; break; }
        case 'd': { cfg.stoponerror = false; break; }

        /* missing option argument */
        case ':': {
          debug(LOG_WARNING) << argv[0] << ": option `-" << optopt << "' requires an argument, going with default value" << std::endl;
          break;
        }

        /* invalid option */
        case '?':
        default:
            // std::cerr << argv[0] << ": option -" << (char)optopt << " is invalid and is ignored" << std::endl;
            return 0;
            break;


    }
  }

  // Set loglevel
  GlobalLogLevel = cfg.loglevel;

  // std::cout << "argc: " << argc << std::endl;
  // std::cout << "optind: " << optind << std::endl;

  if(optind < argc) {
    for(int i = optind; i<argc; i++) {
      // std::cout << "Analyzeing argument: " << argv[i] << std::endl;

      std::filesystem::path path(argv[i]);
      std::error_code error_code; // For using the non-throwing overloads of functions below.

      // Check if argument is a directory
      if (std::filesystem::is_directory(path, error_code)) {
        for(auto itEntry = std::filesystem::recursive_directory_iterator(argv[i]);
             itEntry != std::filesystem::recursive_directory_iterator();
             ++itEntry )
        {
          // Note: Recursive is regulated by depth()
          if(!cfg.recursive && itEntry.depth() > 0)
            continue;

          if (itEntry->is_regular_file())
            AddToQue(itEntry->path(), cfg, queue);

        }
      }

      if (error_code) {
          debug(LOG_ERROR) << "Error: " << error_code.message();
          return 0;
      }



      if (std::filesystem::is_regular_file(path, error_code))
        AddToQue(path, cfg, queue);


      if (error_code) {
          debug(LOG_ERROR) << "Error: " << error_code.message();
          return 0;
      }

    }
  }

  if(queue.empty())
    debug(LOG_INFO) << "No files found!" << std::endl;

  return 1;
};

void CompareOutput(MP3& A_mp3, MP3& B_mp3)
{
  // Print compared data (Highlight diffrence)
  std::string A_filename = A_mp3.m_File->getPath().filename();
  std::string B_filename = B_mp3.m_File->getPath().filename();

  // 19 + 16 + 5
  // ---------------------------------------------------------------------------------------

  std::stringstream DurationString;
  DurationString << "Duration: " << std::setw(19-10) << "" << std::setfill('0') << std::setw(2)
  << A_mp3.calculatedDuration.minutes << ":" << std::setfill('0') << std::setw(2)
  << A_mp3.calculatedDuration.seconds << std::setfill(' ')
  << "" << std::setw(16-5) << "" << std::setfill('0') << std::setw(2) << B_mp3.calculatedDuration.minutes << ":" << std::setfill('0') << std::setw(2) << B_mp3.calculatedDuration.seconds;

  // ---------------------------------------------------------------------------------------

  std::stringstream VersionString;
  VersionString << "Version: " << std::setw(19-9) << "";
  switch (A_mp3.mpegHeader.version) {
    case 1: VersionString << "MPEG Version 1"; break;
    case 2: VersionString << "MPEG Version 2"; break;
    case 25: VersionString << "MPEG Version 2.5"; break;
  }
  VersionString << std::setw((19+16)-VersionString.str().size()) << "";
  switch (B_mp3.mpegHeader.version) {
    case 1: VersionString << "MPEG Version 1"; break;
    case 2: VersionString << "MPEG Version 2"; break;
    case 25: VersionString << "MPEG Version 2.5"; break;
  }

  // ---------------------------------------------------------------------------------------

  std::stringstream LayerString;
  LayerString << "Layer: " << std::setw(19-7) << "";
  switch (A_mp3.mpegHeader.layer) {
    case 1: LayerString << "Layer I"; break;
    case 2: LayerString << "Layer II"; break;
    case 3: LayerString << "Layer III"; break;
  }
  // LayerString << std::setw(10) << "";
  LayerString << std::setw((19+16)-LayerString.str().size()) << "";
  switch (B_mp3.mpegHeader.layer) {
    case 1: LayerString << "Layer I"; break;
    case 2: LayerString << "Layer II"; break;
    case 3: LayerString << "Layer III"; break;
  }

  // ---------------------------------------------------------------------------------------

  std::stringstream BitrateString;
  BitrateString << "Bitrate: " << std::setw(19-9) << "" << A_mp3.mpegHeader.bitrate;
  BitrateString << std::setw((19+16)-BitrateString.str().size()) << "" << B_mp3.mpegHeader.bitrate;

  // ---------------------------------------------------------------------------------------

  std::stringstream SampleingRateString;
  SampleingRateString << "Sample rate: " << std::setw(19-13) << "" << A_mp3.mpegHeader.samplingRate;
  SampleingRateString << std::setw((19+16)-SampleingRateString.str().size()) << "" << B_mp3.mpegHeader.samplingRate;

  // ---------------------------------------------------------------------------------------

  std::stringstream SamplesPerFrameString;
  SamplesPerFrameString << "Samples per Frame: " << std::setw(19-19) << "" << A_mp3.mpegHeader.samplesPerFrame;
  SamplesPerFrameString << std::setw((19+16)-SamplesPerFrameString.str().size()) << "" << B_mp3.mpegHeader.samplesPerFrame;

  // ---------------------------------------------------------------------------------------

  std::stringstream protectionEnabledString;
  protectionEnabledString << "Protected by CRC: " << std::setw(19-18) << "" << ((A_mp3.mpegHeader.protectionEnabled) ? "Yes" : "No") << ((A_mp3.mpegHeader.CRCpassed) ? "(passed)" : "");
  protectionEnabledString << std::setw((19+16)-protectionEnabledString.str().size()) << "";
  protectionEnabledString << ((B_mp3.mpegHeader.protectionEnabled) ? "Yes" : "No") << ((B_mp3.mpegHeader.CRCpassed) ? "(passed)" : "");

  // ---------------------------------------------------------------------------------------

  std::stringstream isPaddedString;
  isPaddedString << "isPadded: " << std::setw(19-10) << "" << ((A_mp3.mpegHeader.isPadded) ? "Yes" : "No");
  isPaddedString << std::setw((19+16)-isPaddedString.str().size()) << "";
  isPaddedString << ((B_mp3.mpegHeader.isPadded) ? "Yes" : "No");

  // ---------------------------------------------------------------------------------------

  std::stringstream isCopyrightedString;
  isCopyrightedString << "isCopyrighted: " << std::setw(19-15) << "" << ((A_mp3.mpegHeader.isCopyrighted) ? "Yes" : "No");
  isCopyrightedString << std::setw((19+16)-isCopyrightedString.str().size()) << "";
  isCopyrightedString << ((B_mp3.mpegHeader.isCopyrighted) ? "Yes" : "No");

  // ---------------------------------------------------------------------------------------

  std::stringstream isOriginalString;
  isOriginalString << "isOriginal: " << std::setw(19-12) << "" << ((A_mp3.mpegHeader.isOriginal) ? "Yes" : "No");
  isOriginalString << std::setw((19+16)-isOriginalString.str().size()) << "";
  isOriginalString << ((B_mp3.mpegHeader.isOriginal) ? "Yes" : "No");

  // ---------------------------------------------------------------------------------------

  std::stringstream xingTag;
  std::stringstream xingFrames;
  std::stringstream xingBytes;
  std::stringstream xingQuality;

  xingTag << "Xing tag: " << std::setw(19-10) << "" << ((A_mp3.mpegHeader.xingHeader) ? A_mp3.mpegHeader.xingString : "No");
  xingTag << std::setw((19+16)-xingTag.str().size()) << "";
  xingTag << ((B_mp3.mpegHeader.xingHeader) ? B_mp3.mpegHeader.xingString : "No");

  if(A_mp3.mpegHeader.xingHeader || B_mp3.mpegHeader.xingHeader)
  {
    xingFrames << "Xing Frames: " << std::setw(19-13) << "" << ((A_mp3.mpegHeader.xingHeader) ? A_mp3.mpegHeader.xing_frames : 0);
    xingFrames << std::setw((19+16)-xingFrames.str().size()) << "";
    xingFrames << ((B_mp3.mpegHeader.xingHeader) ? B_mp3.mpegHeader.xing_frames : 0);

    xingBytes << "Xing Bytes: " << std::setw(19-12) << "" << ((A_mp3.mpegHeader.xingHeader) ? A_mp3.mpegHeader.xing_bytes : 0);
    xingBytes << std::setw((19+16)-xingBytes.str().size()) << "";
    xingBytes << ((B_mp3.mpegHeader.xingHeader) ? B_mp3.mpegHeader.xing_bytes : 0);

    xingQuality << "Xing Quality: " << std::setw(19-14) << "" << ((A_mp3.mpegHeader.xingHeader) ? A_mp3.mpegHeader.xing_quality : 0);
    xingQuality << std::setw((19+16)-xingQuality.str().size()) << "";
    xingQuality << ((B_mp3.mpegHeader.xingHeader) ? B_mp3.mpegHeader.xing_quality : 0);
  }

  // ---------------------------------------------------------------------------------------

  std::stringstream vbriHeaderString;
  std::stringstream vbriVersionString;
  std::stringstream vbriDelayString;
  std::stringstream vbriQualityString;
  std::stringstream vbriBytesString;
  std::stringstream vbriFramesString;
  std::stringstream vbriTOCEntriesString;
  std::stringstream vbriTOCScaleString;
  std::stringstream vbriTOCSizeTableString;
  std::stringstream vbriTOCFrameString;
  std::stringstream vbriTOCSizeString;

  vbriHeaderString << "VBRI tag: " << std::setw(19-10) << "" << ((A_mp3.mpegHeader.VBRIHeader) ? "Yes" : "No");
  vbriHeaderString << std::setw((19+16)-vbriHeaderString.str().size()) << "";
  vbriHeaderString << ((B_mp3.mpegHeader.VBRIHeader) ? "Yes" : "No");

  if(A_mp3.mpegHeader.VBRIHeader || B_mp3.mpegHeader.VBRIHeader)
  {
    vbriVersionString << "VBRI Version: " << std::setw(19-14) << "" << ((A_mp3.mpegHeader.VBRI_version) ? A_mp3.mpegHeader.VBRI_version : 0);
    vbriVersionString << std::setw((19+16)-vbriVersionString.str().size()) << "";
    vbriVersionString << ((B_mp3.mpegHeader.VBRI_version) ? B_mp3.mpegHeader.VBRI_version : 0);

    vbriDelayString << "VBRI Delay: " << std::setw(19-12) << "" << ((A_mp3.mpegHeader.VBRI_delay) ? A_mp3.mpegHeader.VBRI_delay : 0);
    vbriDelayString << std::setw((19+16)-vbriDelayString.str().size()) << "";
    vbriDelayString << ((B_mp3.mpegHeader.VBRI_delay) ? B_mp3.mpegHeader.VBRI_delay : 0);

    vbriQualityString << "VBRI Quality: " << std::setw(19-14) << "" << ((A_mp3.mpegHeader.VBRI_quality) ? A_mp3.mpegHeader.VBRI_quality : 0);
    vbriQualityString << std::setw((19+16)-vbriQualityString.str().size()) << "";
    vbriQualityString << ((B_mp3.mpegHeader.VBRI_quality) ? B_mp3.mpegHeader.VBRI_quality : 0);

    vbriBytesString << "VBRI Number of Bytes: " << std::setw(19-22) << "" << ((A_mp3.mpegHeader.VBRI_bytes) ? A_mp3.mpegHeader.VBRI_bytes : 0);
    vbriBytesString << std::setw((19+16)-vbriBytesString.str().size()) << "";
    vbriBytesString << ((B_mp3.mpegHeader.VBRI_bytes) ? B_mp3.mpegHeader.VBRI_bytes : 0);

    vbriFramesString << "VBRI Number of Frames: " << std::setw(19-23) << "" << ((A_mp3.mpegHeader.VBRI_frames) ? A_mp3.mpegHeader.VBRI_frames : 0);
    vbriFramesString << std::setw((19+16)-vbriFramesString.str().size()) << "";
    vbriFramesString << ((B_mp3.mpegHeader.VBRI_frames) ? B_mp3.mpegHeader.VBRI_frames : 0);

    vbriTOCEntriesString << "VBRI TOC Entries: " << std::setw(19-18) << "" << ((A_mp3.mpegHeader.VBRI_TOC_entries) ? A_mp3.mpegHeader.VBRI_TOC_entries : 0);
    vbriTOCEntriesString << std::setw((19+16)-vbriTOCEntriesString.str().size()) << "";
    vbriTOCEntriesString << ((B_mp3.mpegHeader.VBRI_TOC_entries) ? B_mp3.mpegHeader.VBRI_TOC_entries : 0);

    vbriTOCScaleString << "VBRI TOC Scale: " << std::setw(19-16) << "" << ((A_mp3.mpegHeader.VBRI_TOC_scale) ? A_mp3.mpegHeader.VBRI_TOC_scale : 0);
    vbriTOCScaleString << std::setw((19+16)-vbriTOCScaleString.str().size()) << "";
    vbriTOCScaleString << ((B_mp3.mpegHeader.VBRI_TOC_scale) ? B_mp3.mpegHeader.VBRI_TOC_scale : 0);

    vbriTOCSizeTableString << "VBRI TOC Size / table: " << std::setw(19-23) << "" << ((A_mp3.mpegHeader.VBRI_TOC_sizetable) ? A_mp3.mpegHeader.VBRI_TOC_sizetable : 0);
    vbriTOCSizeTableString << std::setw((19+16)-vbriTOCSizeTableString.str().size()) << "";
    vbriTOCSizeTableString << ((B_mp3.mpegHeader.VBRI_TOC_sizetable) ? B_mp3.mpegHeader.VBRI_TOC_sizetable : 0);

    vbriTOCFrameString << "VBRI TOC Frames / table entry: " << std::setw(19-31) << "" << ((A_mp3.mpegHeader.VBRI_TOC_framestable) ? A_mp3.mpegHeader.VBRI_TOC_framestable : 0);
    vbriTOCFrameString << std::setw((19+16)-vbriTOCFrameString.str().size()) << "";
    vbriTOCFrameString << ((B_mp3.mpegHeader.VBRI_TOC_framestable) ? B_mp3.mpegHeader.VBRI_TOC_framestable : 0);

    vbriTOCSizeString << "VBRI TOC Size: " << std::setw(19-15) << "" << ((A_mp3.mpegHeader.VBRI_TOC_size) ? A_mp3.mpegHeader.VBRI_TOC_size : 0);
    vbriTOCSizeString << std::setw((19+16)-vbriTOCSizeString.str().size()) << "";
    vbriTOCSizeString << ((B_mp3.mpegHeader.VBRI_TOC_size) ? B_mp3.mpegHeader.VBRI_TOC_size : 0);

  }

  std::stringstream id3v2String;
  std::stringstream id3v1String;
  std::stringstream apev1String;
  std::stringstream apev2String;

  if(A_mp3.id3v1_offset || B_mp3.id3v1_offset) {
    id3v1String << "ID3v1 Tag: " << std::setw(19-11) << "" << ((A_mp3.id3v1_offset) ? "Yes" : "No");
    id3v1String << std::setw((19+16)-id3v1String.str().size()) << "";
    id3v1String << ((B_mp3.id3v1_offset) ? "Yes" : "No");
  }

  if(A_mp3.id3v2_offset || B_mp3.id3v2_offset) {
    id3v2String << "ID3v2 Tag: " << std::setw(19-11) << "" << ((A_mp3.id3v2_offset) ? "Yes" : "No");
    id3v2String << std::setw((19+16)-id3v2String.str().size()) << "";
    id3v2String << ((B_mp3.id3v2_offset) ? "Yes" : "No");
  }


  if(A_mp3.apev1_offset || B_mp3.apev1_offset) {
    apev1String << "APEv1 Tag: " << std::setw(19-11) << "" << ((A_mp3.apev1_offset) ? "Yes" : "No");
    apev1String << std::setw((19+16)-apev1String.str().size()) << "";
    apev1String << ((B_mp3.apev1_offset) ? "Yes" : "No");
  }


  if(A_mp3.apev2_offset || B_mp3.apev2_offset) {
    apev2String << "APEv2 Tag: " << std::setw(19-11) << "" << ((A_mp3.apev2_offset) ? "Yes" : "No");
    apev2String << std::setw((19+16)-apev2String.str().size()) << "";
    apev2String << ((B_mp3.apev2_offset) ? "Yes" : "No");
  }


  // ---------------------------------------------------------------------------------------

  debug(LOG_INFO) << "Comparing\n" << CONSOLE_COLOR_U << "(A) " << A_filename << "\n" << "(B) " << B_filename << CONSOLE_COLOR_NORMAL << std::endl;
  // debug(LOG_INFO) << "------------------------------------------------------------------------" << std::endl;
  debug(LOG_INFO) << std::setw(20) << "A" << std::setw(16) << "B" << std::endl;
  debug(LOG_INFO) << DurationString.str() << std::endl;
  debug(LOG_INFO) << VersionString.str() << std::endl;
  debug(LOG_INFO) << LayerString.str() << std::endl;
  debug(LOG_INFO) << BitrateString.str() << std::endl;
  debug(LOG_INFO) << SampleingRateString.str() << std::endl;
  debug(LOG_INFO) << SamplesPerFrameString.str() << std::endl;
  debug(LOG_INFO) << protectionEnabledString.str() << std::endl;
  debug(LOG_INFO) << isPaddedString.str() << std::endl;
  debug(LOG_INFO) << isCopyrightedString.str() << std::endl;
  debug(LOG_INFO) << isOriginalString.str() << std::endl;

  debug(LOG_INFO) << xingTag.str() << std::endl;
  if(A_mp3.mpegHeader.xingHeader || B_mp3.mpegHeader.xingHeader) {
    debug(LOG_INFO) << xingFrames.str() << std::endl;
    debug(LOG_INFO) << xingBytes.str() << std::endl;
    debug(LOG_INFO) << xingQuality.str() << std::endl;
  }

  debug(LOG_INFO) << vbriHeaderString.str() << std::endl;
  if(A_mp3.mpegHeader.VBRIHeader || B_mp3.mpegHeader.VBRIHeader) {
    debug(LOG_INFO) << vbriVersionString.str() << std::endl;
    debug(LOG_INFO) << vbriDelayString.str() << std::endl;
    debug(LOG_INFO) << vbriQualityString.str() << std::endl;
    debug(LOG_INFO) << vbriBytesString.str() << std::endl;
    debug(LOG_INFO) << vbriFramesString.str() << std::endl;
    debug(LOG_INFO) << vbriTOCEntriesString.str() << std::endl;
    debug(LOG_INFO) << vbriTOCScaleString.str() << std::endl;
    debug(LOG_INFO) << vbriTOCSizeTableString.str() << std::endl;
    debug(LOG_INFO) << vbriTOCFrameString.str() << std::endl;
    debug(LOG_INFO) << vbriTOCSizeString.str() << std::endl;
  }

  // TAGS ...
  if(A_mp3.id3v1_offset || B_mp3.id3v1_offset)
    debug(LOG_INFO) << id3v1String.str() << std::endl;

  if(A_mp3.id3v2_offset || B_mp3.id3v2_offset)
    debug(LOG_INFO) << id3v2String.str() << std::endl;

  if(A_mp3.apev1_offset || B_mp3.apev1_offset)
    debug(LOG_INFO) << apev1String.str() << std::endl;

  if(A_mp3.apev2_offset || B_mp3.apev2_offset)
    debug(LOG_INFO) << apev2String.str() << std::endl;

  // ERRORS!?
  std::stringstream errorString;
  if(A_mp3.m_badframes || B_mp3.m_badframes) {

    errorString << "Bad Frames: " << std::setw(19-12) << "";
    if(A_mp3.m_badframes > 0)
      errorString << CONSOLE_COLOR_RED << A_mp3.m_badframes << CONSOLE_COLOR_NORMAL;
    else
      errorString << "No";

    // +13 is for compensating for COLOR macros (i think)
    errorString << std::setw((19+16)-errorString.str().size()+13) << "";

    if(B_mp3.m_badframes > 0)
      errorString << CONSOLE_COLOR_RED << B_mp3.m_badframes << CONSOLE_COLOR_NORMAL;
    else
      errorString << "No";

  } else {
    errorString << "Bad Frames: " << std::setw(19-12) << "" <<  "No" << std::setw(16) << "No";
  }

  debug(LOG_INFO) << errorString.str() << std::endl;

}


int main(int argc, char *argv[])
{

  myConfig cfg;
  std::vector<std::filesystem::path> queue = {};

  // populate config and que according to incoming arguments
  if(HandleArgumentsRequest(argc, argv, cfg, queue)) {
    // Now we should have a config with settings and que with one or more paths


    // SINGLE FILE MODE
    // --------------------------------------------------------
    if(queue.size() == 1) {
      std::cout << "SINGLE FILE MODE" << std::endl;
      cfg.batchmode = false;

      MP3 mp3(queue[0], cfg);
      exit(EXIT_SUCCESS);
    }

    // COMPARE MODE
    // --------------------------------------------------------
    if(queue.size() == 2) {
      std::cout << "COMPARE MODE" << std::endl;

      // in Compare mode, always analys whole files
      cfg.stoponerror = false;
      cfg.batchmode = false;

      // Shsss shsss. quiet
      GlobalLogLevel = LOG_SILENT;

      MP3 A_mp3(queue[0], cfg);
      MP3 B_mp3(queue[1], cfg);

      GlobalLogLevel = cfg.loglevel;

      // Construct and output comparision
      CompareOutput(A_mp3, B_mp3);

      exit(EXIT_SUCCESS);
    }

    // BATCHMODE FILE MODE
    // --------------------------------------------------------
    std::cout << "BATCHMODE" << std::endl;

    if(cfg.stopbatchmode)
      cfg.batchmode = false;

    unsigned index = 0;

    for(auto p : queue){
      index++;
      std::cout << "(" << index << "/" << queue.size() << ")";

      MP3 mp3(p, cfg);

      // Create error output file options is set
      if(cfg.erroroutput) {

        if(mp3.errors){
          // This file has errors!

          // Check if we have an output stream, create one if otherwise
          if(!cfg.errorourputStream) {
            cfg.errorourputStream = new std::ofstream(cfg.erroroutputpath.c_str());

            // Check for errors ..
            if((cfg.errorourputStream->rdstate() & std::ofstream::failbit) != 0) {
              cfg.erroroutput = false;
              debug(LOG_ERROR) << "Failed creating output file.." << std::endl;
            }
          }

          // Give absolute path to ouput stream
          if(cfg.errorourputStream)
            *cfg.errorourputStream << std::filesystem::absolute(mp3.getPath()).string() << std::endl;

        }
      }


    }
    std::cout << std::endl;

    // Clean up
    if(cfg.errorourputStream) {
      if(cfg.errorourputStream->is_open())
        cfg.errorourputStream->close();

      delete cfg.errorourputStream;
    }

  } else {
    usage();
  }

}
