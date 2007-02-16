// This file may be redistributed and modified only under the terms of
// the GNU Lesser General Public License (See COPYING for details).
// Copyright (C) 2005 - 2007 Simon Goodall

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include <getopt.h>

#include <sigc++/bind.h>

#include <libwfut/WFUT.h>
#include <libwfut/Encoder.h>

using namespace WFUT;

static const bool debug = true;

// getopt long argument struct
static struct option long_options [] =
{
  { "update", 1, 0, 'u' },
  { "system", 1, 0, 's' },
  { "server", 1, 0, 'S' },
  { "prefix", 1, 0, 'p' },
  { "version", 0, 0, 'v' },

};

// getopt short argument struct. One char per command
// follow by a : to indicate that an argument it required
static char short_options [] = "u:s:p:vS:";

// Function to check to see if a file exists or not.
// TODO: we could replace this as the loader functions return 1 when they are
// unable to open a file and 2 on failure to parse.
static bool file_exists(const std::string &filename) {
  FILE * fp = fopen(filename.c_str(), "r");
  if (fp == 0) return false;
  fclose(fp);
  return true;

}

static void recordUpdate(const FileObject &fo, const std::string &tmpfile) {
  FILE *fp = 0;
  if (!file_exists(tmpfile)) {
    // Write header 
    fp = fopen(tmpfile.c_str(), "wt");
    if (!fp) {
      //error
      return;
    }
    fprintf(fp, "<?xml version=\"1.0\"?>\n");
    fprintf(fp, "<fileList dir=\"\">\n");
  } else {
    fp = fopen(tmpfile.c_str(), "at");
    if (!fp) {
      //error
      return;
    }
  }
  fprintf(fp, "<file filename=\"%s\" version=\"%d\" crc32=\"%lu\" size=\"%ld\" execute=\"%s\"/>\n", Encoder::encodeString(fo.filename).c_str(), fo.version, fo.crc32, fo.size, (fo.execute) ? ("true") : ("false"));
  fclose(fp);

}

// Signal handler called when a file is sicessfully downloaded
// We use this to update the local file list with the updated details
void onDownloadComplete(const std::string &u, const std::string &f, const ChannelFileList &updates, ChannelFileList *local, const std::string &tmpfile)  {
  printf("Downloaded %s\n", f.c_str());

  const WFUT::FileMap &ulist = updates.getFiles();
  WFUT::FileMap::const_iterator I = ulist.find(f);
  // The fileobject should exist, otherwise how did we get here?
  assert (I != ulist.end());
  // Add the updated file record to the local list.
  // addFile will overwrite any existing fileobject with the same
  // filename
  local->addFile(I->second);

  // TODO: We should store in a tmp file the fact that we sucessfully downloaded
  // this file, incase of a crash.
  recordUpdate(I->second, tmpfile);
}

// Signal handler called when a download fails.
void onDownloadFailed(const std::string &u, const std::string &f, const std::string &r, int *error)  {
  fprintf(stderr,"Error downloading %s\n", u.c_str());
  // Increment error count
  ++error;
}

int main(int argc, char *argv[]) {

  // Set some default values which we can override with command line parameters.
  std::string server_root = "http://white.worldforge.org/downloads/WFUT/";
  std::string channel_file = "wfut.xml";
  std::string tmpfile = "tempwfut.xml";

  std::string channel = ".";
  std::string local_path = "./";
  std::string system_path = "";
 
  while (true) {
    int opt_index = 0;
    int c = getopt_long(argc, argv, short_options, long_options, &opt_index);
    if (c == -1) break;
    switch (c) {
      case 'v':
        fprintf(stderr, "WFUT Version: %s\n", VERSION);
        exit (0);
        break;
      case 'u':
        if (optarg) {
          channel = optarg;
        } else {
          fprintf(stderr, "Missing channel name\n");
        }
        break;
       case 's':
        if (optarg) {
          system_path = optarg;
        } else {
          fprintf(stderr, "Missing system path\n");
        }
        break;
       case 'p':
        if (optarg) {
          local_path = optarg;
        } else {
          fprintf(stderr, "Missing prefix\n");
        }
        break;
       case 'S':
        if (optarg) {
          server_root = optarg;
        } else {
          fprintf(stderr, "Missing system path\n");
        }
        break;
 
      default:
        fprintf(stderr, "Unknown command: %c\n", c);
    }
  }

  if (debug) printf("Channel name: %s\n", channel.c_str());
  // This is the base path for all files that will be downloaded
  const std::string &local_root = local_path + "/" + channel + "/";

  WFUTClient wfut;
  // Initialise wfut. This does the curl setup.
  wfut.init();

  // Define the channelfilelist objects we will use
  ChannelFileList local, system, server, updates, tmplist;


  // Look for local wfut file.
  const std::string &local_wfut = local_path  + "/" + channel + "/" + channel_file;
  if (debug) printf("Local wfut: %s\n", local_wfut.c_str());

  if (file_exists(local_wfut)) {
    if (wfut.getLocalList(local_wfut, local)) {
      fprintf(stderr, "Error reading local wfut.xml file\n");
      wfut.shutdown();
      return 1;
    } else {
      if (channel == ".") channel = local.getName();
    }
  }

  // Look for tmpwfut file. If it exists, update the local files list.
  const std::string &tmp_wfut = local_path  + "/" + tmpfile;
  if (debug) printf("Tmp wfut: %s\n", tmp_wfut.c_str());

  if (file_exists(tmp_wfut)) {
    if (wfut.getLocalList(tmp_wfut, tmplist)) {
      fprintf(stderr, "Error reading tmpwfut.xml file\n");
      wfut.shutdown();
      return 1;
    } else {
      const FileMap &fm = tmplist.getFiles();
      FileMap::const_iterator I = fm.begin();
      FileMap::const_iterator Iend = fm.end();
      for (; I != Iend; ++I) {
        local.addFile(I->second);
      }
    }
  }


  // Look for a system wfut file
  if (!system_path.empty()) {
  const std::string &system_wfut = system_path + "/" + channel + "/" + channel_file; 
  if (debug) printf("System wfut: %s\n", system_wfut.c_str());

  if (file_exists(system_wfut)) {
    if (wfut.getLocalList(system_wfut, system)) {
      fprintf(stderr, "Error reading system wfut.xml file\n");
      wfut.shutdown();
      return 1;
    } else {
      if (channel == ".") channel = system.getName();
    }
  }
  }
  // By now we should have a proper channel name. If not, then there is nothing 
  // we can do to find the server updates.
  if (channel.empty() || channel == ".") {
    fprintf(stderr, "Unable to determine channel name.\n");
    wfut.shutdown();
    return 1;
  }

  const std::string &server_wfut = server_root + "/" + channel + "/" + channel_file; 
  if (debug) printf("Server wfut: %s\n", server_wfut.c_str());

  if (wfut.getFileList(server_wfut, server)) {
    printf("Error downloading server channel file\n");
    wfut.shutdown();
    return 1;
  }

  if (debug) printf("Local Root: %s\n", local_root.c_str());

  printf("Updating Channel: %s\n", channel.c_str());

  // Now we have loaded all our data files, lets find out what we really need
  // to download
  if (wfut.calculateUpdates(server, system, local, updates, local_root)) {
    printf("Error determining file to update\n");
    wfut.shutdown();
    return 1;
  }

  // Make sure the local file has the correct channel name
  local.setName(server.getName());

  // Queue the list of files to download
  wfut.updateChannel(updates, server_root + "/" + channel, local_root);

  // error counts the number of download failures. This is incremented in the
  // download failed signal handler
  int error = 0;
  // Connect up the signal handlers
  wfut.DownloadComplete.connect(sigc::bind(sigc::ptr_fun(onDownloadComplete), updates, &local, tmp_wfut));
  wfut.DownloadFailed.connect(sigc::bind(sigc::ptr_fun(onDownloadFailed), &error));

  // Keep polling to download some more file bits.
  while (wfut.poll());

  // Save the completed download list
  wfut.saveLocalList(local, local_wfut);

  // Clean up WFUT. Closes curl handles
  wfut.shutdown();

  if (error) {
    fprintf(stderr, "%d files failed to download.\n", error);
  }

  // Return error. Will be 0 on success
  return error;
}
