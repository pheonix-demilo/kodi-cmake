/*
 *      Copyright (C) 2005-2013 Team XBMC
 *      http://xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "network/Network.h"
#include "system.h"
#include "utils/AlarmClock.h"
#include "utils/Screenshot.h"
#include "Application.h"
#include "ApplicationMessenger.h"
#include "Autorun.h"
#include "Builtins.h"
#include "input/ButtonTranslator.h"
#include "FileItem.h"
#include "addons/GUIDialogAddonSettings.h"
#include "dialogs/GUIDialogFileBrowser.h"
#include "guilib/GUIKeyboardFactory.h"
#include "guilib/Key.h"
#include "guilib/StereoscopicsManager.h"
#include "dialogs/GUIDialogKaiToast.h"
#include "dialogs/GUIDialogNumeric.h"
#include "dialogs/GUIDialogProgress.h"
#include "dialogs/GUIDialogYesNo.h"
#include "GUIUserMessages.h"
#include "windows/GUIWindowLoginScreen.h"
#include "video/windows/GUIWindowVideoBase.h"
#include "addons/GUIWindowAddonBrowser.h"
#include "addons/Addon.h" // for TranslateType, TranslateContent
#include "addons/AddonInstaller.h"
#include "addons/AddonManager.h"
#include "addons/PluginSource.h"
#include "interfaces/generic/ScriptInvocationManager.h"
#include "interfaces/AnnouncementManager.h"
#include "network/NetworkServices.h"
#include "utils/log.h"
#include "storage/MediaManager.h"
#include "utils/RssManager.h"
#include "utils/JSONVariantParser.h"
#include "PartyModeManager.h"
#include "profiles/ProfilesManager.h"
#include "settings/DisplaySettings.h"
#include "settings/Settings.h"
#include "settings/MediaSettings.h"
#include "settings/MediaSourceSettings.h"
#include "settings/SkinSettings.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "Util.h"
#include "URL.h"
#include "music/MusicDatabase.h"
#include "cores/IPlayer.h"

#include "filesystem/PluginDirectory.h"
#include "filesystem/ZipManager.h"

#include "guilib/GUIWindowManager.h"
#include "guilib/LocalizeStrings.h"

#ifdef HAS_LIRC
#include "input/linux/LIRC.h"
#endif
#ifdef HAS_IRSERVERSUITE

  #include "input/windows/IRServerSuite.h"

#endif

#if defined(TARGET_DARWIN)
#include "filesystem/SpecialProtocol.h"
#include "osx/CocoaInterface.h"
#endif

#ifdef HAS_CDDA_RIPPER
#include "cdrip/CDDARipper.h"
#endif

#include <vector>
#include "settings/AdvancedSettings.h"
#include "settings/DisplaySettings.h"

using namespace std;
using namespace XFILE;
using namespace ADDON;

#ifdef HAS_DVD_DRIVE
using namespace MEDIA_DETECT;
#endif

typedef struct
{
  const char* command;
  bool needsParameters;
  const char* description;
} BUILT_IN;

const BUILT_IN commands[] = {
  { "Help",                       false,  "This help message" },
  { "Reboot",                     false,  "Reboot the system" },
  { "Restart",                    false,  "Restart the system (same as reboot)" },
  { "ShutDown",                   false,  "Shutdown the system" },
  { "Powerdown",                  false,  "Powerdown system" },
  { "Quit",                       false,  "Quit XBMC" },
  { "Hibernate",                  false,  "Hibernates the system" },
  { "Suspend",                    false,  "Suspends the system" },
  { "InhibitIdleShutdown",        false,  "Inhibit idle shutdown" },
  { "AllowIdleShutdown",          false,  "Allow idle shutdown" },
  { "ActivateScreensaver",        false,  "Activate Screensaver" },
  { "RestartApp",                 false,  "Restart XBMC" },
  { "Minimize",                   false,  "Minimize XBMC" },
  { "Reset",                      false,  "Reset the system (same as reboot)" },
  { "Mastermode",                 false,  "Control master mode" },
  { "SetGUILanguage",             true,   "Set GUI Language" },
  { "ActivateWindow",             true,   "Activate the specified window" },
  { "ActivateWindowAndFocus",     true,   "Activate the specified window and sets focus to the specified id" },
  { "ReplaceWindowAndFocus",      true,   "Replaces the current window with the new one and sets focus to the specified id" },
  { "ReplaceWindow",              true,   "Replaces the current window with the new one" },
  { "TakeScreenshot",             false,  "Takes a Screenshot" },
  { "RunScript",                  true,   "Run the specified script" },
  { "StopScript",                 true,   "Stop the script by ID or path, if running" },
#if defined(TARGET_DARWIN)
  { "RunAppleScript",             true,   "Run the specified AppleScript command" },
#endif
  { "RunPlugin",                  true,   "Run the specified plugin" },
  { "RunAddon",                   true,   "Run the specified plugin/script" },
  { "NotifyAll",                  true,   "Notify all connected clients" },
  { "Extract",                    true,   "Extracts the specified archive" },
  { "PlayMedia",                  true,   "Play the specified media file (or playlist)" },
  { "SlideShow",                  true,   "Run a slideshow from the specified directory" },
  { "RecursiveSlideShow",         true,   "Run a slideshow from the specified directory, including all subdirs" },
  { "ReloadSkin",                 false,  "Reload XBMC's skin" },
  { "UnloadSkin",                 false,  "Unload XBMC's skin" },
  { "RefreshRSS",                 false,  "Reload RSS feeds from RSSFeeds.xml"},
  { "PlayerControl",              true,   "Control the music or video player" },
  { "Playlist.PlayOffset",        true,   "Start playing from a particular offset in the playlist" },
  { "Playlist.Clear",             false,  "Clear the current playlist" },
  { "EjectTray",                  false,  "Close or open the DVD tray" },
  { "AlarmClock",                 true,   "Prompt for a length of time and start an alarm clock" },
  { "CancelAlarm",                true,   "Cancels an alarm" },
  { "Action",                     true,   "Executes an action for the active window (same as in keymap)" },
  { "Notification",               true,   "Shows a notification on screen, specify header, then message, and optionally time in milliseconds and a icon." },
  { "PlayDVD",                    false,  "Plays the inserted CD or DVD media from the DVD-ROM Drive!" },
  { "RipCD",                      false,  "Rip the currently inserted audio CD"},
  { "Skin.ToggleSetting",         true,   "Toggles a skin setting on or off" },
  { "Skin.SetString",             true,   "Prompts and sets skin string" },
  { "Skin.SetNumeric",            true,   "Prompts and sets numeric input" },
  { "Skin.SetPath",               true,   "Prompts and sets a skin path" },
  { "Skin.Theme",                 true,   "Control skin theme" },
  { "Skin.SetImage",              true,   "Prompts and sets a skin image" },
  { "Skin.SetLargeImage",         true,   "Prompts and sets a large skin images" },
  { "Skin.SetFile",               true,   "Prompts and sets a file" },
  { "Skin.SetAddon",              true,   "Prompts and set an addon" },
  { "Skin.SetBool",               true,   "Sets a skin setting on" },
  { "Skin.Reset",                 true,   "Resets a skin setting to default" },
  { "Skin.ResetSettings",         false,  "Resets all skin settings" },
  { "Mute",                       false,  "Mute the player" },
  { "SetVolume",                  true,   "Set the current volume" },
  { "Dialog.Close",               true,   "Close a dialog" },
  { "System.LogOff",              false,  "Log off current user" },
  { "System.Exec",                true,   "Execute shell commands" },
  { "System.ExecWait",            true,   "Execute shell commands and freezes XBMC until shell is closed" },
  { "Resolution",                 true,   "Change XBMC's Resolution" },
  { "SetFocus",                   true,   "Change current focus to a different control id" },
  { "UpdateLibrary",              true,   "Update the selected library (music or video)" },
  { "CleanLibrary",               true,   "Clean the video/music library" },
  { "ExportLibrary",              true,   "Export the video/music library" },
  { "PageDown",                   true,   "Send a page down event to the pagecontrol with given id" },
  { "PageUp",                     true,   "Send a page up event to the pagecontrol with given id" },
  { "Container.Refresh",          false,  "Refresh current listing" },
  { "Container.Update",           false,  "Update current listing. Send Container.Update(path,replace) to reset the path history" },
  { "Container.NextViewMode",     false,  "Move to the next view type (and refresh the listing)" },
  { "Container.PreviousViewMode", false,  "Move to the previous view type (and refresh the listing)" },
  { "Container.SetViewMode",      true,   "Move to the view with the given id" },
  { "Container.NextSortMethod",   false,  "Change to the next sort method" },
  { "Container.PreviousSortMethod",false, "Change to the previous sort method" },
  { "Container.SetSortMethod",    true,   "Change to the specified sort method" },
  { "Container.SortDirection",    false,  "Toggle the sort direction" },
  { "Control.Move",               true,   "Tells the specified control to 'move' to another entry specified by offset" },
  { "Control.SetFocus",           true,   "Change current focus to a different control id" },
  { "Control.Message",            true,   "Send a given message to a control within a given window" },
  { "SendClick",                  true,   "Send a click message from the given control to the given window" },
  { "LoadProfile",                true,   "Load the specified profile (note; if locks are active it won't work)" },
  { "SetProperty",                true,   "Sets a window property for the current focused window/dialog (key,value)" },
  { "ClearProperty",              true,   "Clears a window property for the current focused window/dialog (key,value)" },
  { "PlayWith",                   true,   "Play the selected item with the specified core" },
  { "WakeOnLan",                  true,   "Sends the wake-up packet to the broadcast address for the specified MAC address" },
  { "Addon.Default.OpenSettings", true,   "Open a settings dialog for the default addon of the given type" },
  { "Addon.Default.Set",          true,   "Open a select dialog to allow choosing the default addon of the given type" },
  { "Addon.OpenSettings",         true,   "Open a settings dialog for the addon of the given id" },
  { "UpdateAddonRepos",           false,  "Check add-on repositories for updates" },
  { "UpdateLocalAddons",          false,  "Check for local add-on changes" },
  { "ToggleDPMS",                 false,  "Toggle DPMS mode manually"},
  { "CECToggleState",             false,  "Toggle state of playing device via a CEC peripheral"},
  { "CECActivateSource",          false,  "Wake up playing device via a CEC peripheral"},
  { "CECStandby",                 false,  "Put playing device on standby via a CEC peripheral"},
  { "Weather.Refresh",            false,  "Force weather data refresh"},
  { "Weather.LocationNext",       false,  "Switch to next weather location"},
  { "Weather.LocationPrevious",   false,  "Switch to previous weather location"},
  { "Weather.LocationSet",        true,   "Switch to given weather location (parameter can be 1-3)"},
#if defined(HAS_LIRC) || defined(HAS_IRSERVERSUITE)
  { "LIRC.Stop",                  false,  "Removes XBMC as LIRC client" },
  { "LIRC.Start",                 false,  "Adds XBMC as LIRC client" },
  { "LIRC.Send",                  true,   "Sends a command to LIRC" },
#endif
  { "VideoLibrary.Search",        false,  "Brings up a search dialog which will search the library" },
  { "ToggleDebug",                false,  "Enables/disables debug mode" },
  { "StartPVRManager",            false,  "(Re)Starts the PVR manager" },
  { "StopPVRManager",             false,  "Stops the PVR manager" },
#if defined(TARGET_ANDROID)
  { "StartAndroidActivity",       true,   "Launch an Android native app with the given package name.  Optional parms (in order): intent, dataType, dataURI." },
#endif
  { "SetStereoMode",              true,   "Changes the stereo mode of the GUI. Params can be: toggle, next, previous, select, tomono or any of the supported stereomodes (off, split_vertical, split_horizontal, row_interleaved, hardware_based, anaglyph_cyan_red, anaglyph_green_magenta, monoscopic)" }
};

bool CBuiltins::HasCommand(const CStdString& execString)
{
  CStdString function;
  vector<CStdString> parameters;
  CUtil::SplitExecFunction(execString, function, parameters);
  for (unsigned int i = 0; i < sizeof(commands)/sizeof(BUILT_IN); i++)
  {
    if (StringUtils::EqualsNoCase(function, commands[i].command) && (!commands[i].needsParameters || parameters.size()))
      return true;
  }
  return false;
}

void CBuiltins::GetHelp(CStdString &help)
{
  help.clear();
  for (unsigned int i = 0; i < sizeof(commands)/sizeof(BUILT_IN); i++)
  {
    help += commands[i].command;
    help += "\t";
    help += commands[i].description;
    help += "\n";
  }
}

int CBuiltins::Execute(const CStdString& execString)
{
  // Get the text after the "XBMC."
  CStdString execute;
  vector<CStdString> params;
  CUtil::SplitExecFunction(execString, execute, params);
  StringUtils::ToLower(execute);
  CStdString parameter = params.size() ? params[0] : "";
  CStdString strParameterCaseIntact = parameter;

  if (execute.Equals("reboot") || execute.Equals("restart") || execute.Equals("reset"))  //Will reboot the system
  {
    CApplicationMessenger::Get().Restart();
  }
  else if (execute.Equals("shutdown"))
  {
    CApplicationMessenger::Get().Shutdown();
  }
  else if (execute.Equals("powerdown"))
  {
    CApplicationMessenger::Get().Powerdown();
  }
  else if (execute.Equals("restartapp"))
  {
    CApplicationMessenger::Get().RestartApp();
  }
  else if (execute.Equals("hibernate"))
  {
    CApplicationMessenger::Get().Hibernate();
  }
  else if (execute.Equals("suspend"))
  {
    CApplicationMessenger::Get().Suspend();
  }
  else if (execute.Equals("quit"))
  {
    CApplicationMessenger::Get().Quit();
  }
  else if (execute.Equals("inhibitidleshutdown"))
  {
    bool inhibit = (params.size() == 1 && params[0].Equals("true"));
    CApplicationMessenger::Get().InhibitIdleShutdown(inhibit);
  }
  else if (execute.Equals("activatescreensaver"))
  {
    CApplicationMessenger::Get().ActivateScreensaver();
  }
  else if (execute.Equals("minimize"))
  {
    CApplicationMessenger::Get().Minimize();
  }
  else if (execute.Equals("loadprofile"))
  {
    int index = CProfilesManager::Get().GetProfileIndex(parameter);
    bool prompt = (params.size() == 2 && params[1].Equals("prompt"));
    bool bCanceled;
    if (index >= 0
        && (CProfilesManager::Get().GetMasterProfile().getLockMode() == LOCK_MODE_EVERYONE
            || g_passwordManager.IsProfileLockUnlocked(index,bCanceled,prompt)))
    {
      CApplicationMessenger::Get().LoadProfile(index);
    }
  }
  else if (execute.Equals("mastermode"))
  {
    if (g_passwordManager.bMasterUser)
    {
      g_passwordManager.bMasterUser = false;
      g_passwordManager.LockSources(true);
      CGUIDialogKaiToast::QueueNotification(CGUIDialogKaiToast::Warning, g_localizeStrings.Get(20052),g_localizeStrings.Get(20053));
    }
    else if (g_passwordManager.IsMasterLockUnlocked(true))
    {
      g_passwordManager.LockSources(false);
      g_passwordManager.bMasterUser = true;
      CGUIDialogKaiToast::QueueNotification(CGUIDialogKaiToast::Warning, g_localizeStrings.Get(20052),g_localizeStrings.Get(20054));
    }

    CUtil::DeleteVideoDatabaseDirectoryCache();
    CGUIMessage msg(GUI_MSG_NOTIFY_ALL, 0, 0, GUI_MSG_UPDATE);
    g_windowManager.SendMessage(msg);
  }
  else if (execute.Equals("setguilanguage"))
  {
    if (params.size())
    {
      CApplicationMessenger::Get().SetGUILanguage(params[0]);
    }
  }
  else if (execute.Equals("takescreenshot"))
  {
    if (params.size())
    {
      // get the parameters
      CStdString strSaveToPath = params[0];
      bool sync = false;
      if (params.size() >= 2)
        sync = params[1].Equals("sync");

      if (!strSaveToPath.empty())
      {
        if (CDirectory::Exists(strSaveToPath))
        {
          CStdString file = CUtil::GetNextFilename(URIUtils::AddFileToFolder(strSaveToPath, "screenshot%03d.png"), 999);

          if (!file.empty())
          {
            CScreenShot::TakeScreenshot(file, sync);
          }
          else
          {
            CLog::Log(LOGWARNING, "Too many screen shots or invalid folder %s", strSaveToPath.c_str());
          }
        }
        else
          CScreenShot::TakeScreenshot(strSaveToPath, sync);
      }
    }
    else
      CScreenShot::TakeScreenshot();
  }
  else if (execute.Equals("activatewindow") || execute.Equals("replacewindow"))
  {
    // get the parameters
    CStdString strWindow;
    if (params.size())
    {
      strWindow = params[0];
      params.erase(params.begin());
    }

    // confirm the window destination is valid prior to switching
    int iWindow = CButtonTranslator::TranslateWindow(strWindow);
    if (iWindow != WINDOW_INVALID)
    {
      // disable the screensaver
      g_application.WakeUpScreenSaverAndDPMS();
      g_windowManager.ActivateWindow(iWindow, params, !execute.Equals("activatewindow"));
    }
    else
    {
      CLog::Log(LOGERROR, "Activate/ReplaceWindow called with invalid destination window: %s", strWindow.c_str());
      return false;
    }
  }
  else if ((execute.Equals("setfocus") || execute.Equals("control.setfocus")) && params.size())
  {
    int controlID = atol(params[0].c_str());
    int subItem = (params.size() > 1) ? atol(params[1].c_str())+1 : 0;
    CGUIMessage msg(GUI_MSG_SETFOCUS, g_windowManager.GetFocusedWindow(), controlID, subItem);
    g_windowManager.SendMessage(msg);
  }
  else if ((execute.Equals("activatewindowandfocus") || execute.Equals("replacewindowandfocus")) && params.size())
  {
    CStdString strWindow = params[0];

    // confirm the window destination is valid prior to switching
    int iWindow = CButtonTranslator::TranslateWindow(strWindow);
    if (iWindow != WINDOW_INVALID)
    {
      // disable the screensaver
      g_application.WakeUpScreenSaverAndDPMS();
      vector<CStdString> dummy;
      g_windowManager.ActivateWindow(iWindow, dummy, !execute.Equals("activatewindowandfocus"));

      unsigned int iPtr = 1;
      while (params.size() > iPtr + 1)
      {
        CGUIMessage msg(GUI_MSG_SETFOCUS, g_windowManager.GetFocusedWindow(),
            atol(params[iPtr].c_str()),
            (params.size() >= iPtr + 2) ? atol(params[iPtr + 1].c_str())+1 : 0);
        g_windowManager.SendMessage(msg);
        iPtr += 2;
      }
    }
    else
    {
      CLog::Log(LOGERROR, "Replace/ActivateWindowAndFocus called with invalid destination window: %s", strWindow.c_str());
      return false;
    }
  }
  else if (execute.Equals("runscript") && params.size())
  {
#if defined(TARGET_DARWIN_OSX)
    if (URIUtils::HasExtension(strParameterCaseIntact, ".applescript|.scpt"))
    {
      CStdString osxPath = CSpecialProtocol::TranslatePath(strParameterCaseIntact);
      Cocoa_DoAppleScriptFile(osxPath.c_str());
    }
    else
#endif
    {
      vector<string> argv;
      for (vector<CStdString>::const_iterator param = params.begin(); param != params.end(); ++param)
        argv.push_back(*param);

      vector<CStdString> path;
      //split the path up to find the filename
      StringUtils::SplitString(params[0],"\\",path);
      if (path.size())
        argv[0] = path[path.size() - 1];

      AddonPtr script;
      CStdString scriptpath(params[0]);
      if (CAddonMgr::Get().GetAddon(params[0], script))
        scriptpath = script->LibPath();

      CScriptInvocationManager::Get().Execute(scriptpath, script, argv);
    }
  }
#if defined(TARGET_DARWIN_OSX)
  else if (execute.Equals("runapplescript"))
  {
    Cocoa_DoAppleScript(strParameterCaseIntact.c_str());
  }
#endif
  else if (execute.Equals("stopscript"))
  {
    CStdString scriptpath(params[0]);

    // Test to see if the param is an addon ID
    AddonPtr script;
    if (CAddonMgr::Get().GetAddon(params[0], script))
      scriptpath = script->LibPath();

    CScriptInvocationManager::Get().Stop(scriptpath);
  }
  else if (execute.Equals("system.exec"))
  {
    CApplicationMessenger::Get().Minimize();
    CApplicationMessenger::Get().ExecOS(parameter, false);
  }
  else if (execute.Equals("system.execwait"))
  {
    CApplicationMessenger::Get().Minimize();
    CApplicationMessenger::Get().ExecOS(parameter, true);
  }
  else if (execute.Equals("resolution"))
  {
    RESOLUTION res = RES_PAL_4x3;
    if (parameter.Equals("pal")) res = RES_PAL_4x3;
    else if (parameter.Equals("pal16x9")) res = RES_PAL_16x9;
    else if (parameter.Equals("ntsc")) res = RES_NTSC_4x3;
    else if (parameter.Equals("ntsc16x9")) res = RES_NTSC_16x9;
    else if (parameter.Equals("720p")) res = RES_HDTV_720p;
    else if (parameter.Equals("720pSBS")) res = RES_HDTV_720pSBS;
    else if (parameter.Equals("720pTB")) res = RES_HDTV_720pTB;
    else if (parameter.Equals("1080pSBS")) res = RES_HDTV_1080pSBS;
    else if (parameter.Equals("1080pTB")) res = RES_HDTV_1080pTB;
    else if (parameter.Equals("1080i")) res = RES_HDTV_1080i;
    if (g_graphicsContext.IsValidResolution(res))
    {
      CDisplaySettings::Get().SetCurrentResolution(res, true);
      g_application.ReloadSkin();
    }
  }
  else if (execute.Equals("extract") && params.size())
  {
    // Detects if file is zip or rar then extracts
    CStdString strDestDirect;
    if (params.size() < 2)
      strDestDirect = URIUtils::GetDirectory(params[0]);
    else
      strDestDirect = params[1];

    URIUtils::AddSlashAtEnd(strDestDirect);


    CFileItemList items;
    CFileItemPtr ptr(new CFileItem());
    CStdString archpath;
    URIUtils::CreateArchivePath(archpath, URIUtils::GetExtension(archpath).substr(1), params[0], "");
    ptr->SetPath(archpath);
    ptr->Select(true);
    CFileOperationJob job(CFileOperationJob::ActionCopy, items, strDestDirect);
    if (!job.DoWork())
      CLog::Log(LOGERROR, "XBMC.Extract, Error extracting archive");
  }
  else if (execute.Equals("runplugin"))
  {
    if (params.size())
    {
      CFileItem item(params[0]);
      if (!item.m_bIsFolder)
      {
        item.SetPath(params[0]);
        CPluginDirectory::RunScriptWithParams(item.GetPath());
      }
    }
    else
    {
      CLog::Log(LOGERROR, "XBMC.RunPlugin called with no arguments.");
    }
  }
  else if (execute.Equals("runaddon"))
  {
    if (params.size())
    {
      AddonPtr addon;
      CStdString cmd;
      if (CAddonMgr::Get().GetAddon(params[0],addon,ADDON_PLUGIN))
      {
        PluginPtr plugin = boost::dynamic_pointer_cast<CPluginSource>(addon);
        CStdString addonid = params[0];
        CStdString urlParameters;
        CStdStringArray parameters;
        if (params.size() == 2 &&
           (StringUtils::StartsWith(params[1], "/") || StringUtils::StartsWith(params[1], "?")))
          urlParameters = params[1];
        else if (params.size() > 1)
        {
          parameters.insert(parameters.begin(), params.begin() + 1, params.end());
          urlParameters = "?" + StringUtils::JoinString(parameters, "&");
        }
        else
        {
          // Add '/' if addon is run without params (will be removed later so it's safe)
          // Otherwise there are 2 entries for the same plugin in ViewModesX.db
          urlParameters = "/";
        }

        if (plugin->Provides(CPluginSource::VIDEO))
          cmd = StringUtils::Format("ActivateWindow(Videos,plugin://%s%s,return)", addonid.c_str(), urlParameters.c_str());
        else if (plugin->Provides(CPluginSource::AUDIO))
          cmd = StringUtils::Format("ActivateWindow(Music,plugin://%s%s,return)", addonid.c_str(), urlParameters.c_str());
        else if (plugin->Provides(CPluginSource::EXECUTABLE))
          cmd = StringUtils::Format("ActivateWindow(Programs,plugin://%s%s,return)", addonid.c_str(), urlParameters.c_str());
        else if (plugin->Provides(CPluginSource::IMAGE))
          cmd = StringUtils::Format("ActivateWindow(Pictures,plugin://%s%s,return)", addonid.c_str(), urlParameters.c_str());
        else
          // Pass the script name (params[0]) and all the parameters
          // (params[1] ... params[x]) separated by a comma to RunPlugin
          cmd = StringUtils::Format("RunPlugin(%s)", StringUtils::JoinString(params, ",").c_str());
      }
      else if (CAddonMgr::Get().GetAddon(params[0], addon, ADDON_SCRIPT) ||
               CAddonMgr::Get().GetAddon(params[0], addon, ADDON_SCRIPT_WEATHER) ||
               CAddonMgr::Get().GetAddon(params[0], addon, ADDON_SCRIPT_LYRICS))
        // Pass the script name (params[0]) and all the parameters
        // (params[1] ... params[x]) separated by a comma to RunScript
        cmd = StringUtils::Format("RunScript(%s)", StringUtils::JoinString(params, ",").c_str());

      return Execute(cmd);
    }
    else
    {
      CLog::Log(LOGERROR, "XBMC.RunAddon called with no arguments.");
    }
  }
  else if (execute.Equals("notifyall"))
  {
    if (params.size() > 1)
    {
      CVariant data;
      if (params.size() > 2)
        data = CJSONVariantParser::Parse((const unsigned char *)params[2].c_str(), params[2].size());

      ANNOUNCEMENT::CAnnouncementManager::Announce(ANNOUNCEMENT::Other, params[0], params[1], data);
    }
    else
      CLog::Log(LOGERROR, "XBMC.NotifyAll needs two parameters");
  }
  else if (execute.Equals("playmedia"))
  {
    if (!params.size())
    {
      CLog::Log(LOGERROR, "XBMC.PlayMedia called with empty parameter");
      return -3;
    }

    CFileItem item(params[0], false);
    if (URIUtils::HasSlashAtEnd(params[0]))
      item.m_bIsFolder = true;

    // restore to previous window if needed
    if( g_windowManager.GetActiveWindow() == WINDOW_SLIDESHOW ||
        g_windowManager.GetActiveWindow() == WINDOW_FULLSCREEN_VIDEO ||
        g_windowManager.GetActiveWindow() == WINDOW_VISUALISATION )
        g_windowManager.PreviousWindow();

    // reset screensaver
    g_application.ResetScreenSaver();
    g_application.WakeUpScreenSaverAndDPMS();

    // ask if we need to check guisettings to resume
    bool askToResume = true;
    int playOffset = 0;
    for (unsigned int i = 1 ; i < params.size() ; i++)
    {
      if (params[i].Equals("isdir"))
        item.m_bIsFolder = true;
      else if (params[i].Equals("1")) // set fullscreen or windowed
        CMediaSettings::Get().SetVideoStartWindowed(true);
      else if (params[i].Equals("resume"))
      {
        // force the item to resume (if applicable) (see CApplication::PlayMedia)
        item.m_lStartOffset = STARTOFFSET_RESUME;
        askToResume = false;
      }
      else if (params[i].Equals("noresume"))
      {
        // force the item to start at the beginning (m_lStartOffset is initialized to 0)
        askToResume = false;
      }
      else if (StringUtils::StartsWithNoCase(params[i], "playoffset=")) {
        playOffset = atoi(params[i].substr(11).c_str()) - 1;
        item.SetProperty("playlist_starting_track", playOffset);
      }
    }

    if (!item.m_bIsFolder && item.IsPlugin())
      item.SetProperty("IsPlayable", true);

    if ( askToResume == true )
    {
      if ( CGUIWindowVideoBase::ShowResumeMenu(item) == false )
        return false;
    }
    if (item.m_bIsFolder)
    {
      CFileItemList items;
      CStdString extensions = g_advancedSettings.m_videoExtensions + "|" + g_advancedSettings.m_musicExtensions;
      CDirectory::GetDirectory(item.GetPath(),items,extensions);
      
      bool containsMusic = false, containsVideo = false;
      for (int i = 0; i < items.Size(); i++)
      {
        bool isVideo = items[i]->IsVideo();
        containsMusic |= !isVideo;
        containsVideo |= isVideo;
        
        if (containsMusic && containsVideo)
          break;
      }
      
      int playlist = containsVideo? PLAYLIST_VIDEO : PLAYLIST_MUSIC;;
      if (containsMusic && containsVideo) //mixed content found in the folder
      {
        for (int i = items.Size() - 1; i >= 0; i--) //remove music entries
        {
          if (!items[i]->IsVideo())
            items.Remove(i);
        }
      }
      
      g_playlistPlayer.ClearPlaylist(playlist);
      g_playlistPlayer.Add(playlist, items);
      g_playlistPlayer.SetCurrentPlaylist(playlist);
      g_playlistPlayer.Play(playOffset);
    }
    else
    {
      int playlist = item.IsAudio() ? PLAYLIST_MUSIC : PLAYLIST_VIDEO;
      g_playlistPlayer.ClearPlaylist(playlist);
      g_playlistPlayer.SetCurrentPlaylist(playlist);

      // play media
      if (!g_application.PlayMedia(item, playlist))
      {
        CLog::Log(LOGERROR, "XBMC.PlayMedia could not play media: %s", params[0].c_str());
        return false;
      }
    }
  }
  else if (execute.Equals("showPicture"))
  {
    if (!params.size())
    {
      CLog::Log(LOGERROR, "XBMC.ShowPicture called with empty parameter");
      return -2;
    }
    CGUIMessage msg(GUI_MSG_SHOW_PICTURE, 0, 0);
    msg.SetStringParam(params[0]);
    CGUIWindow *pWindow = g_windowManager.GetWindow(WINDOW_SLIDESHOW);
    if (pWindow) pWindow->OnMessage(msg);
  }
  else if (execute.Equals("slideShow") || execute.Equals("recursiveslideShow"))
  {
    if (!params.size())
    {
      CLog::Log(LOGERROR, "XBMC.SlideShow called with empty parameter");
      return -2;
    }
    std::string beginSlidePath;
    // leave RecursiveSlideShow command as-is
    unsigned int flags = 0;
    if (execute.Equals("RecursiveSlideShow"))
      flags |= 1;

    // SlideShow(dir[,recursive][,[not]random][,pause][,beginslide="/path/to/start/slide.jpg"])
    // the beginslide value need be escaped (for '"' or '\' in it, by backslash)
    // and then quoted, or not. See CUtil::SplitParams()
    else
    {
      for (unsigned int i = 1 ; i < params.size() ; i++)
      {
        if (params[i].Equals("recursive"))
          flags |= 1;
        else if (params[i].Equals("random")) // set fullscreen or windowed
          flags |= 2;
        else if (params[i].Equals("notrandom"))
          flags |= 4;
        else if (params[i].Equals("pause"))
          flags |= 8;
        else if (StringUtils::StartsWithNoCase(params[i], "beginslide="))
          beginSlidePath = params[i].substr(11);
      }
    }

    CGUIMessage msg(GUI_MSG_START_SLIDESHOW, 0, 0, flags);
    vector<CStdString> strParams;
    strParams.push_back(params[0]);
    strParams.push_back(beginSlidePath);
    msg.SetStringParams(strParams);
    CGUIWindow *pWindow = g_windowManager.GetWindow(WINDOW_SLIDESHOW);
    if (pWindow) pWindow->OnMessage(msg);
  }
  else if (execute.Equals("reloadskin"))
  {
    //  Reload the skin
    g_application.ReloadSkin(!params.empty() && StringUtils::EqualsNoCase(params[0], "confirm"));
  }
  else if (execute.Equals("unloadskin"))
  {
    g_application.UnloadSkin(true); // we're reloading the skin after this
  }
  else if (execute.Equals("refreshrss"))
  {
    CRssManager::Get().Reload();
  }
  else if (execute.Equals("playercontrol"))
  {
    g_application.ResetScreenSaver();
    g_application.WakeUpScreenSaverAndDPMS();
    if (!params.size())
    {
      CLog::Log(LOGERROR, "XBMC.PlayerControl called with empty parameter");
      return -3;
    }
    if (parameter.Equals("play"))
    { // play/pause
      // either resume playing, or pause
      if (g_application.m_pPlayer->IsPlaying())
      {
        if (g_application.m_pPlayer->GetPlaySpeed() != 1)
          g_application.m_pPlayer->SetPlaySpeed(1, g_application.IsMutedInternal());
        else
          g_application.m_pPlayer->Pause();
      }
    }
    else if (parameter.Equals("stop"))
    {
      g_application.StopPlaying();
    }
    else if (parameter.Equals("rewind") || parameter.Equals("forward"))
    {
      if (g_application.m_pPlayer->IsPlaying() && !g_application.m_pPlayer->IsPaused())
      {
        int iPlaySpeed = g_application.m_pPlayer->GetPlaySpeed();
        if (parameter.Equals("rewind") && iPlaySpeed == 1) // Enables Rewinding
          iPlaySpeed *= -2;
        else if (parameter.Equals("rewind") && iPlaySpeed > 1) //goes down a notch if you're FFing
          iPlaySpeed /= 2;
        else if (parameter.Equals("forward") && iPlaySpeed < 1) //goes up a notch if you're RWing
        {
          iPlaySpeed /= 2;
          if (iPlaySpeed == -1) iPlaySpeed = 1;
        }
        else
          iPlaySpeed *= 2;

        if (iPlaySpeed > 32 || iPlaySpeed < -32)
          iPlaySpeed = 1;

        g_application.m_pPlayer->SetPlaySpeed(iPlaySpeed, g_application.IsMutedInternal());
      }
    }
    else if (parameter.Equals("next"))
    {
      g_application.OnAction(CAction(ACTION_NEXT_ITEM));
    }
    else if (parameter.Equals("previous"))
    {
      g_application.OnAction(CAction(ACTION_PREV_ITEM));
    }
    else if (parameter.Equals("bigskipbackward"))
    {
      if (g_application.m_pPlayer->IsPlaying())
        g_application.m_pPlayer->Seek(false, true);
    }
    else if (parameter.Equals("bigskipforward"))
    {
      if (g_application.m_pPlayer->IsPlaying())
        g_application.m_pPlayer->Seek(true, true);
    }
    else if (parameter.Equals("smallskipbackward"))
    {
      if (g_application.m_pPlayer->IsPlaying())
        g_application.m_pPlayer->Seek(false, false);
    }
    else if (parameter.Equals("smallskipforward"))
    {
      if (g_application.m_pPlayer->IsPlaying())
        g_application.m_pPlayer->Seek(true, false);
    }
    else if (StringUtils::StartsWithNoCase(parameter, "seekpercentage"))
    {
      CStdString offset = "";
      if (parameter.size() == 14)
        CLog::Log(LOGERROR,"PlayerControl(seekpercentage(n)) called with no argument");
      else if (parameter.size() < 17) // arg must be at least "(N)"
        CLog::Log(LOGERROR,"PlayerControl(seekpercentage(n)) called with invalid argument: \"%s\"", parameter.substr(14).c_str());
      else
      {
        // Don't bother checking the argument: an invalid arg will do seek(0)
        offset = parameter.substr(15);
        StringUtils::TrimRight(offset, ")");
        float offsetpercent = (float) atof(offset.c_str());
        if (offsetpercent < 0 || offsetpercent > 100)
          CLog::Log(LOGERROR,"PlayerControl(seekpercentage(n)) argument, %f, must be 0-100", offsetpercent);
        else if (g_application.m_pPlayer->IsPlaying())
          g_application.SeekPercentage(offsetpercent);
      }
    }
    else if( parameter.Equals("showvideomenu") )
    {
      if( g_application.m_pPlayer->IsPlaying() )
        g_application.m_pPlayer->OnAction(CAction(ACTION_SHOW_VIDEOMENU));
    }
    else if( parameter.Equals("record") )
    {
      if( g_application.m_pPlayer->IsPlaying() && g_application.m_pPlayer->CanRecord())
        g_application.m_pPlayer->Record(!g_application.m_pPlayer->IsRecording());
    }
    else if (StringUtils::StartsWithNoCase(parameter, "partymode"))
    {
      CStdString strXspPath = "";
      //empty param=music, "music"=music, "video"=video, else xsp path
      PartyModeContext context = PARTYMODECONTEXT_MUSIC;
      if (parameter.size() > 9)
      {
        if (parameter.size() == 16 && StringUtils::EndsWithNoCase(parameter, "video)"))
          context = PARTYMODECONTEXT_VIDEO;
        else if (parameter.size() != 16 || !StringUtils::EndsWithNoCase(parameter, "music)"))
        {
          strXspPath = parameter.substr(10);
          StringUtils::TrimRight(strXspPath, ")");
          context = PARTYMODECONTEXT_UNKNOWN;
        }
      }
      if (g_partyModeManager.IsEnabled())
        g_partyModeManager.Disable();
      else
        g_partyModeManager.Enable(context, strXspPath);
    }
    else if (parameter.Equals("random")    ||
             parameter.Equals("randomoff") ||
             parameter.Equals("randomon"))
    {
      // get current playlist
      int iPlaylist = g_playlistPlayer.GetCurrentPlaylist();

      // reverse the current setting
      bool shuffled = g_playlistPlayer.IsShuffled(iPlaylist);
      if ((shuffled && parameter.Equals("randomon")) || (!shuffled && parameter.Equals("randomoff")))
        return 0;

      // check to see if we should notify the user
      bool notify = (params.size() == 2 && params[1].Equals("notify"));
      g_playlistPlayer.SetShuffle(iPlaylist, !shuffled, notify);

      // save settings for now playing windows
      switch (iPlaylist)
      {
      case PLAYLIST_MUSIC:
        CMediaSettings::Get().SetMusicPlaylistShuffled(g_playlistPlayer.IsShuffled(iPlaylist));
        CSettings::Get().Save();
        break;
      case PLAYLIST_VIDEO:
        CMediaSettings::Get().SetVideoPlaylistShuffled(g_playlistPlayer.IsShuffled(iPlaylist));
        CSettings::Get().Save();
      }

      // send message
      CGUIMessage msg(GUI_MSG_PLAYLISTPLAYER_RANDOM, 0, 0, iPlaylist, g_playlistPlayer.IsShuffled(iPlaylist));
      g_windowManager.SendThreadMessage(msg);

    }
    else if (StringUtils::StartsWithNoCase(parameter, "repeat"))
    {
      // get current playlist
      int iPlaylist = g_playlistPlayer.GetCurrentPlaylist();
      PLAYLIST::REPEAT_STATE previous_state = g_playlistPlayer.GetRepeat(iPlaylist);

      PLAYLIST::REPEAT_STATE state;
      if (parameter.Equals("repeatall"))
        state = PLAYLIST::REPEAT_ALL;
      else if (parameter.Equals("repeatone"))
        state = PLAYLIST::REPEAT_ONE;
      else if (parameter.Equals("repeatoff"))
        state = PLAYLIST::REPEAT_NONE;
      else if (previous_state == PLAYLIST::REPEAT_NONE)
        state = PLAYLIST::REPEAT_ALL;
      else if (previous_state == PLAYLIST::REPEAT_ALL)
        state = PLAYLIST::REPEAT_ONE;
      else
        state = PLAYLIST::REPEAT_NONE;

      if (state == previous_state)
        return 0;

      // check to see if we should notify the user
      bool notify = (params.size() == 2 && params[1].Equals("notify"));
      g_playlistPlayer.SetRepeat(iPlaylist, state, notify);

      // save settings for now playing windows
      switch (iPlaylist)
      {
      case PLAYLIST_MUSIC:
        CMediaSettings::Get().SetMusicPlaylistRepeat(state == PLAYLIST::REPEAT_ALL);
        CSettings::Get().Save();
        break;
      case PLAYLIST_VIDEO:
        CMediaSettings::Get().SetVideoPlaylistRepeat(state == PLAYLIST::REPEAT_ALL);
        CSettings::Get().Save();
      }

      // send messages so now playing window can get updated
      CGUIMessage msg(GUI_MSG_PLAYLISTPLAYER_REPEAT, 0, 0, iPlaylist, (int)state);
      g_windowManager.SendThreadMessage(msg);
    }
  }
  else if (execute.Equals("playwith"))
  {
    g_application.m_eForcedNextPlayer = CPlayerCoreFactory::Get().GetPlayerCore(parameter);
    g_application.OnAction(CAction(ACTION_PLAYER_PLAY));
  }
  else if (execute.Equals("mute"))
  {
    g_application.ToggleMute();
  }
  else if (execute.Equals("setvolume"))
  {
    float oldVolume = g_application.GetVolume();
    float volume = (float)strtod(parameter.c_str(), NULL);

    g_application.SetVolume(volume);
    if(oldVolume != volume)
    {
      if(params.size() > 1 && params[1].Equals("showVolumeBar"))    
      {
        CApplicationMessenger::Get().ShowVolumeBar(oldVolume < volume);  
      }
    }
  }
  else if (execute.Equals("playlist.playoffset"))
  {
    // playlist.playoffset(offset)
    // playlist.playoffset(music|video,offset)
    CStdString strPos = parameter;
    if (params.size() > 1)
    {
      // ignore any other parameters if present
      CStdString strPlaylist = params[0];
      strPos = params[1];

      int iPlaylist = PLAYLIST_NONE;
      if (strPlaylist.Equals("music"))
        iPlaylist = PLAYLIST_MUSIC;
      else if (strPlaylist.Equals("video"))
        iPlaylist = PLAYLIST_VIDEO;

      // unknown playlist
      if (iPlaylist == PLAYLIST_NONE)
      {
        CLog::Log(LOGERROR,"Playlist.PlayOffset called with unknown playlist: %s", strPlaylist.c_str());
        return false;
      }

      // user wants to play the 'other' playlist
      if (iPlaylist != g_playlistPlayer.GetCurrentPlaylist())
      {
        g_application.StopPlaying();
        g_playlistPlayer.Reset();
        g_playlistPlayer.SetCurrentPlaylist(iPlaylist);
      }
    }
    // play the desired offset
    int pos = atol(strPos.c_str());
    // playlist is already playing
    if (g_application.m_pPlayer->IsPlaying())
      g_playlistPlayer.PlayNext(pos);
    // we start playing the 'other' playlist so we need to use play to initialize the player state
    else
      g_playlistPlayer.Play(pos);
  }
  else if (execute.Equals("playlist.clear"))
  {
    g_playlistPlayer.Clear();
  }
#ifdef HAS_DVD_DRIVE
  else if (execute.Equals("ejecttray"))
  {
    g_mediaManager.ToggleTray();
  }
#endif
  else if( execute.Equals("alarmclock") && params.size() > 1 )
  {
    // format is alarmclock(name,command[,seconds,true]);
    float seconds = 0;
    if (params.size() > 2)
    {
      if (params[2].find(':') == std::string::npos)
        seconds = static_cast<float>(atoi(params[2].c_str())*60);
      else
        seconds = (float)StringUtils::TimeStringToSeconds(params[2]);
    }
    else
    { // check if shutdown is specified in particular, and get the time for it
      CStdString strHeading;
      if (StringUtils::EqualsNoCase(parameter, "shutdowntimer"))
        strHeading = g_localizeStrings.Get(20145);
      else
        strHeading = g_localizeStrings.Get(13209);
      CStdString strTime;
      if( CGUIDialogNumeric::ShowAndGetNumber(strTime, strHeading) )
        seconds = static_cast<float>(atoi(strTime.c_str())*60);
      else
        return false;
    }
    bool silent = false;
    bool loop = false;
    for (unsigned int i = 3; i < params.size() ; i++)
    {
      // check "true" for backward comp
      if (StringUtils::EqualsNoCase(params[i], "true") || StringUtils::EqualsNoCase(params[i], "silent"))
        silent = true;
      else if (StringUtils::EqualsNoCase(params[i], "loop"))
        loop = true;
    }

    if( g_alarmClock.IsRunning() )
      g_alarmClock.Stop(params[0],silent);
    // no negative times not allowed, loop must have a positive time
    if (seconds < 0 || (seconds == 0 && loop))
      return false;
    g_alarmClock.Start(params[0], seconds, params[1], silent, loop);
  }
  else if (execute.Equals("notification"))
  {
    if (params.size() < 2)
      return -1;
    if (params.size() == 4)
      CGUIDialogKaiToast::QueueNotification(params[3],params[0],params[1],atoi(params[2].c_str()));
    else if (params.size() == 3)
      CGUIDialogKaiToast::QueueNotification("",params[0],params[1],atoi(params[2].c_str()));
    else
      CGUIDialogKaiToast::QueueNotification(params[0],params[1]);
  }
  else if (execute.Equals("cancelalarm"))
  {
    bool silent = false;
    if (params.size() > 1 && StringUtils::EqualsNoCase(params[1], "true"))
      silent = true;
    g_alarmClock.Stop(params[0],silent);
  }
  else if (execute.Equals("playdvd"))
  {
#ifdef HAS_DVD_DRIVE
    bool restart = false;
    if (params.size() > 0 && StringUtils::EqualsNoCase(params[0], "restart"))
      restart = true;
    CAutorun::PlayDisc(g_mediaManager.GetDiscPath(), true, restart);
#endif
  }
  else if (execute.Equals("ripcd"))
  {
#ifdef HAS_CDDA_RIPPER
    CCDDARipper::GetInstance().RipCD();
#endif
  }
  else if (execute.Equals("skin.togglesetting"))
  {
    int setting = CSkinSettings::Get().TranslateBool(parameter);
    CSkinSettings::Get().SetBool(setting, !CSkinSettings::Get().GetBool(setting));
    CSettings::Get().Save();
  }
  else if (execute.Equals("skin.setbool") && params.size())
  {
    if (params.size() > 1)
    {
      int string = CSkinSettings::Get().TranslateBool(params[0]);
      CSkinSettings::Get().SetBool(string, StringUtils::EqualsNoCase(params[1], "true"));
      CSettings::Get().Save();
      return 0;
    }
    // default is to set it to true
    int setting = CSkinSettings::Get().TranslateBool(params[0]);
    CSkinSettings::Get().SetBool(setting, true);
    CSettings::Get().Save();
  }
  else if (execute.Equals("skin.reset"))
  {
    CSkinSettings::Get().Reset(parameter);
    CSettings::Get().Save();
  }
  else if (execute.Equals("skin.resetsettings"))
  {
    CSkinSettings::Get().Reset();
    CSettings::Get().Save();
  }
  else if (execute.Equals("skin.theme"))
  {
    // enumerate themes
    vector<CStdString> vecTheme;
    CUtil::GetSkinThemes(vecTheme);

    int iTheme = -1;

    // find current theme
    if (!StringUtils::EqualsNoCase(CSettings::Get().GetString("lookandfeel.skintheme"), "SKINDEFAULT"))
    {
      for (unsigned int i=0;i<vecTheme.size();++i)
      {
        CStdString strTmpTheme(CSettings::Get().GetString("lookandfeel.skintheme"));
        URIUtils::RemoveExtension(strTmpTheme);
        if (vecTheme[i].Equals(strTmpTheme))
        {
          iTheme=i;
          break;
        }
      }
    }

    int iParam = atoi(parameter.c_str());
    if (iParam == 0 || iParam == 1)
      iTheme++;
    else if (iParam == -1)
      iTheme--;
    if (iTheme > (int)vecTheme.size()-1)
      iTheme = -1;
    if (iTheme < -1)
      iTheme = vecTheme.size()-1;

    CStdString strSkinTheme = "SKINDEFAULT";
    if (iTheme != -1 && iTheme < (int)vecTheme.size())
      strSkinTheme = vecTheme[iTheme];

    CSettings::Get().SetString("lookandfeel.skintheme", strSkinTheme);
    // also set the default color theme
    CStdString colorTheme(URIUtils::ReplaceExtension(strSkinTheme, ".xml"));
    if (colorTheme.Equals("Textures.xml"))
      colorTheme = "defaults.xml";
    CSettings::Get().SetString("lookandfeel.skincolors", colorTheme);
    g_application.ReloadSkin();
  }
  else if (execute.Equals("skin.setstring") || execute.Equals("skin.setimage") || execute.Equals("skin.setfile") ||
           execute.Equals("skin.setpath") || execute.Equals("skin.setnumeric") || execute.Equals("skin.setlargeimage"))
  {
    // break the parameter up if necessary
    int string = 0;
    if (params.size() > 1)
    {
      string = CSkinSettings::Get().TranslateString(params[0]);
      if (execute.Equals("skin.setstring"))
      {
        CSkinSettings::Get().SetString(string, params[1]);
        CSettings::Get().Save();
        return 0;
      }
    }
    else
      string = CSkinSettings::Get().TranslateString(params[0]);
    CStdString value = CSkinSettings::Get().GetString(string);
    VECSOURCES localShares;
    g_mediaManager.GetLocalDrives(localShares);
    if (execute.Equals("skin.setstring"))
    {
      if (CGUIKeyboardFactory::ShowAndGetInput(value, g_localizeStrings.Get(1029), true))
        CSkinSettings::Get().SetString(string, value);
    }
    else if (execute.Equals("skin.setnumeric"))
    {
      if (CGUIDialogNumeric::ShowAndGetNumber(value, g_localizeStrings.Get(611)))
        CSkinSettings::Get().SetString(string, value);
    }
    else if (execute.Equals("skin.setimage"))
    {
      if (CGUIDialogFileBrowser::ShowAndGetImage(localShares, g_localizeStrings.Get(1030), value))
        CSkinSettings::Get().SetString(string, value);
    }
    else if (execute.Equals("skin.setlargeimage"))
    {
      VECSOURCES *shares = CMediaSourceSettings::Get().GetSources("pictures");
      if (!shares) shares = &localShares;
      if (CGUIDialogFileBrowser::ShowAndGetImage(*shares, g_localizeStrings.Get(1030), value))
        CSkinSettings::Get().SetString(string, value);
    }
    else if (execute.Equals("skin.setfile"))
    {
      // Note. can only browse one addon type from here
      // if browsing for addons, required param[1] is addontype string, with optional param[2]
      // as contenttype string see IAddon.h & ADDON::TranslateXX
      CStdString strMask = (params.size() > 1) ? params[1] : "";
      StringUtils::ToLower(strMask);
      ADDON::TYPE type;
      if ((type = TranslateType(strMask)) != ADDON_UNKNOWN)
      {
        CURL url;
        url.SetProtocol("addons");
        url.SetHostName("enabled");
        url.SetFileName(strMask+"/");
        localShares.clear();
        CStdString content = (params.size() > 2) ? params[2] : "";
        StringUtils::ToLower(content);
        url.SetPassword(content);
        CStdString strMask;
        if (type == ADDON_SCRIPT)
          strMask = ".py";
        CStdString replace;
        if (CGUIDialogFileBrowser::ShowAndGetFile(url.Get(), strMask, TranslateType(type, true), replace, true, true, true))
        {
          if (StringUtils::StartsWithNoCase(replace, "addons://"))
            CSkinSettings::Get().SetString(string, URIUtils::GetFileName(replace));
          else
            CSkinSettings::Get().SetString(string, replace);
        }
      }
      else 
      {
        if (params.size() > 2)
        {
          value = params[2];
          URIUtils::AddSlashAtEnd(value);
          bool bIsSource;
          if (CUtil::GetMatchingSource(value,localShares,bIsSource) < 0) // path is outside shares - add it as a separate one
          {
            CMediaSource share;
            share.strName = g_localizeStrings.Get(13278);
            share.strPath = value;
            localShares.push_back(share);
          }
        }
        if (CGUIDialogFileBrowser::ShowAndGetFile(localShares, strMask, g_localizeStrings.Get(1033), value))
          CSkinSettings::Get().SetString(string, value);
      }
    }
    else // execute.Equals("skin.setpath"))
    {
      g_mediaManager.GetNetworkLocations(localShares);
      if (params.size() > 1)
      {
        value = params[1];
        URIUtils::AddSlashAtEnd(value);
        bool bIsSource;
        if (CUtil::GetMatchingSource(value,localShares,bIsSource) < 0) // path is outside shares - add it as a separate one
        {
          CMediaSource share;
          share.strName = g_localizeStrings.Get(13278);
          share.strPath = value;
          localShares.push_back(share);
        }
      }
      if (CGUIDialogFileBrowser::ShowAndGetDirectory(localShares, g_localizeStrings.Get(1031), value))
        CSkinSettings::Get().SetString(string, value);
    }
    CSettings::Get().Save();
  }
  else if (execute.Equals("skin.setaddon") && params.size() > 1)
  {
    int string = CSkinSettings::Get().TranslateString(params[0]);
    vector<ADDON::TYPE> types;
    for (unsigned int i = 1 ; i < params.size() ; i++)
    {
      ADDON::TYPE type = TranslateType(params[i]);
      if (type != ADDON_UNKNOWN)
        types.push_back(type);
    }
    CStdString result;
    if (types.size() > 0 && CGUIWindowAddonBrowser::SelectAddonID(types, result, true) == 1)
    {
      CSkinSettings::Get().SetString(string, result);
      CSettings::Get().Save();
    }
  }
  else if (execute.Equals("dialog.close") && params.size())
  {
    bool bForce = false;
    if (params.size() > 1 && StringUtils::EqualsNoCase(params[1], "true"))
      bForce = true;
    if (StringUtils::EqualsNoCase(params[0], "all"))
    {
      g_windowManager.CloseDialogs(bForce);
    }
    else
    {
      int id = CButtonTranslator::TranslateWindow(params[0]);
      CGUIWindow *window = (CGUIWindow *)g_windowManager.GetWindow(id);
      if (window && window->IsDialog())
        ((CGUIDialog *)window)->Close(bForce);
    }
  }
  else if (execute.Equals("system.logoff"))
  {
    // there was a commit from cptspiff here which was reverted
    // for keeping the behaviour from Eden in Frodo - see
    // git rev 9ee5f0047b
    if (g_windowManager.GetActiveWindow() == WINDOW_LOGIN_SCREEN)
      return -1;

    g_application.StopPlaying();
    if (g_application.IsMusicScanning())
      g_application.StopMusicScan();

    if (g_application.IsVideoScanning())
      g_application.StopVideoScan();

    ADDON::CAddonMgr::Get().StopServices(true);

    g_application.getNetwork().NetworkMessage(CNetwork::SERVICES_DOWN,1);
    CProfilesManager::Get().LoadMasterProfileForLogin();
    g_passwordManager.bMasterUser = false;
    g_windowManager.ActivateWindow(WINDOW_LOGIN_SCREEN);
    if (!CNetworkServices::Get().StartEventServer()) // event server could be needed in some situations
      CGUIDialogKaiToast::QueueNotification(CGUIDialogKaiToast::Warning, g_localizeStrings.Get(33102), g_localizeStrings.Get(33100));
  }
  else if (execute.Equals("pagedown"))
  {
    int id = atoi(parameter.c_str());
    CGUIMessage message(GUI_MSG_PAGE_DOWN, g_windowManager.GetFocusedWindow(), id);
    g_windowManager.SendMessage(message);
  }
  else if (execute.Equals("pageup"))
  {
    int id = atoi(parameter.c_str());
    CGUIMessage message(GUI_MSG_PAGE_UP, g_windowManager.GetFocusedWindow(), id);
    g_windowManager.SendMessage(message);
  }
  else if (execute.Equals("updatelibrary") && params.size())
  {
    if (params[0].Equals("music"))
    {
      if (g_application.IsMusicScanning())
        g_application.StopMusicScan();
      else
        g_application.StartMusicScan(params.size() > 1 ? params[1] : "");
    }
    if (params[0].Equals("video"))
    {
      if (g_application.IsVideoScanning())
        g_application.StopVideoScan();
      else
        g_application.StartVideoScan(params.size() > 1 ? params[1] : "");
    }
  }
  else if (execute.Equals("cleanlibrary"))
  {
    if (!params.size() || params[0].Equals("video"))
    {
      if (!g_application.IsVideoScanning())
         g_application.StartVideoCleanup();
      else
        CLog::Log(LOGERROR, "XBMC.CleanLibrary is not possible while scanning or cleaning");
    }
    else if (params[0].Equals("music"))
    {
      if (!g_application.IsMusicScanning())
      {
        CMusicDatabase musicdatabase;

        musicdatabase.Open();
        musicdatabase.Cleanup();
        musicdatabase.Close();
      }
      else
        CLog::Log(LOGERROR, "XBMC.CleanLibrary is not possible while scanning for media info");
    }
  }
  else if (execute.Equals("exportlibrary"))
  {
    int iHeading = 647;
    if (params[0].Equals("music"))
      iHeading = 20196;
    CStdString path;
    VECSOURCES shares;
    g_mediaManager.GetLocalDrives(shares);
    g_mediaManager.GetNetworkLocations(shares);
    g_mediaManager.GetRemovableDrives(shares);
    bool singleFile;
    bool thumbs=false;
    bool actorThumbs=false;
    bool overwrite=false;
    bool cancelled=false;

    if (params.size() > 1)
      singleFile = params[1].Equals("true");
    else
      singleFile = CGUIDialogYesNo::ShowAndGetInput(iHeading,20426,20427,-1,20428,20429,cancelled);

    if (cancelled)
        return -1;

    if (singleFile)
    {
      if (params.size() > 2)
        thumbs = params[2].Equals("true");
      else
        thumbs = CGUIDialogYesNo::ShowAndGetInput(iHeading,20430,-1,-1,cancelled);
    }

    if (cancelled)
      return -1;

    if (thumbs && params[0].Equals("video"))
    {
      if (params.size() > 4)
        actorThumbs = params[4].Equals("true");
      else
        actorThumbs = CGUIDialogYesNo::ShowAndGetInput(iHeading,20436,-1,-1,cancelled);
    }

    if (cancelled)
      return -1;

    if (singleFile)
    {
      if (params.size() > 3)
        overwrite = params[3].Equals("true");
      else
        overwrite = CGUIDialogYesNo::ShowAndGetInput(iHeading,20431,-1,-1,cancelled);
    }

    if (cancelled)
      return -1;

    if (params.size() > 2)
      path=params[2];
    if (singleFile || !path.empty() ||
        CGUIDialogFileBrowser::ShowAndGetDirectory(shares,
				  g_localizeStrings.Get(661), path, true))
    {
      if (params[0].Equals("video"))
      {
        CVideoDatabase videodatabase;
        videodatabase.Open();
        videodatabase.ExportToXML(path, singleFile, thumbs, actorThumbs, overwrite);
        videodatabase.Close();
      }
      else
      {
        if (URIUtils::HasSlashAtEnd(path))
          path = URIUtils::AddFileToFolder(path, "musicdb.xml");
        CMusicDatabase musicdatabase;
        musicdatabase.Open();
        musicdatabase.ExportToXML(path, singleFile, thumbs, overwrite);
        musicdatabase.Close();
      }
    }
  }
  else if (execute.Equals("control.move") && params.size() > 1)
  {
    CGUIMessage message(GUI_MSG_MOVE_OFFSET, g_windowManager.GetFocusedWindow(), atoi(params[0].c_str()), atoi(params[1].c_str()));
    g_windowManager.SendMessage(message);
  }
  else if (execute.Equals("container.refresh"))
  { // NOTE: These messages require a media window, thus they're sent to the current activewindow.
    //       This shouldn't stop a dialog intercepting it though.
    CGUIMessage message(GUI_MSG_NOTIFY_ALL, g_windowManager.GetActiveWindow(), 0, GUI_MSG_UPDATE, 1); // 1 to reset the history
    message.SetStringParam(parameter);
    g_windowManager.SendMessage(message);
  }
  else if (execute.Equals("container.update") && params.size())
  {
    CGUIMessage message(GUI_MSG_NOTIFY_ALL, g_windowManager.GetActiveWindow(), 0, GUI_MSG_UPDATE, 0);
    message.SetStringParam(params[0]);
    if (params.size() > 1 && StringUtils::EqualsNoCase(params[1], "replace"))
      message.SetParam2(1); // reset the history
    g_windowManager.SendMessage(message);
  }
  else if (execute.Equals("container.nextviewmode"))
  {
    CGUIMessage message(GUI_MSG_CHANGE_VIEW_MODE, g_windowManager.GetActiveWindow(), 0, 0, 1);
    g_windowManager.SendMessage(message);
  }
  else if (execute.Equals("container.previousviewmode"))
  {
    CGUIMessage message(GUI_MSG_CHANGE_VIEW_MODE, g_windowManager.GetActiveWindow(), 0, 0, -1);
    g_windowManager.SendMessage(message);
  }
  else if (execute.Equals("container.setviewmode"))
  {
    CGUIMessage message(GUI_MSG_CHANGE_VIEW_MODE, g_windowManager.GetActiveWindow(), 0, atoi(parameter.c_str()));
    g_windowManager.SendMessage(message);
  }
  else if (execute.Equals("container.nextsortmethod"))
  {
    CGUIMessage message(GUI_MSG_CHANGE_SORT_METHOD, g_windowManager.GetActiveWindow(), 0, 0, 1);
    g_windowManager.SendMessage(message);
  }
  else if (execute.Equals("container.previoussortmethod"))
  {
    CGUIMessage message(GUI_MSG_CHANGE_SORT_METHOD, g_windowManager.GetActiveWindow(), 0, 0, -1);
    g_windowManager.SendMessage(message);
  }
  else if (execute.Equals("container.setsortmethod"))
  {
    CGUIMessage message(GUI_MSG_CHANGE_SORT_METHOD, g_windowManager.GetActiveWindow(), 0, atoi(parameter.c_str()));
    g_windowManager.SendMessage(message);
  }
  else if (execute.Equals("container.sortdirection"))
  {
    CGUIMessage message(GUI_MSG_CHANGE_SORT_DIRECTION, g_windowManager.GetActiveWindow(), 0, 0);
    g_windowManager.SendMessage(message);
  }
  else if (execute.Equals("control.message") && params.size() >= 2)
  {
    int controlID = atoi(params[0].c_str());
    int windowID = (params.size() == 3) ? CButtonTranslator::TranslateWindow(params[2]) : g_windowManager.GetActiveWindow();
    if (params[1] == "moveup")
      g_windowManager.SendMessage(GUI_MSG_MOVE_OFFSET, windowID, controlID, 1);
    else if (params[1] == "movedown")
      g_windowManager.SendMessage(GUI_MSG_MOVE_OFFSET, windowID, controlID, -1);
    else if (params[1] == "pageup")
      g_windowManager.SendMessage(GUI_MSG_PAGE_UP, windowID, controlID);
    else if (params[1] == "pagedown")
      g_windowManager.SendMessage(GUI_MSG_PAGE_DOWN, windowID, controlID);
    else if (params[1] == "click")
      g_windowManager.SendMessage(GUI_MSG_CLICKED, controlID, windowID);
  }
  else if (execute.Equals("sendclick") && params.size())
  {
    if (params.size() == 2)
    {
      // have a window - convert it
      int windowID = CButtonTranslator::TranslateWindow(params[0]);
      CGUIMessage message(GUI_MSG_CLICKED, atoi(params[1].c_str()), windowID);
      g_windowManager.SendMessage(message);
    }
    else
    { // single param - assume you meant the active window
      CGUIMessage message(GUI_MSG_CLICKED, atoi(params[0].c_str()), g_windowManager.GetActiveWindow());
      g_windowManager.SendMessage(message);
    }
  }
  else if (execute.Equals("action") && params.size())
  {
    // try translating the action from our ButtonTranslator
    int actionID;
    if (CButtonTranslator::TranslateActionString(params[0].c_str(), actionID))
    {
      int windowID = params.size() == 2 ? CButtonTranslator::TranslateWindow(params[1]) : WINDOW_INVALID;
      CApplicationMessenger::Get().SendAction(CAction(actionID), windowID);
    }
  }
  else if (execute.Equals("setproperty") && params.size() >= 2)
  {
    CGUIWindow *window = g_windowManager.GetWindow(params.size() > 2 ? CButtonTranslator::TranslateWindow(params[2]) : g_windowManager.GetFocusedWindow());
    if (window)
      window->SetProperty(params[0],params[1]);
  }
  else if (execute.Equals("clearproperty") && params.size())
  {
    CGUIWindow *window = g_windowManager.GetWindow(params.size() > 1 ? CButtonTranslator::TranslateWindow(params[1]) : g_windowManager.GetFocusedWindow());
    if (window)
      window->SetProperty(params[0],"");
  }
  else if (execute.Equals("wakeonlan"))
  {
    g_application.getNetwork().WakeOnLan((char*)params[0].c_str());
  }
  else if (execute.Equals("addon.default.opensettings") && params.size() == 1)
  {
    AddonPtr addon;
    ADDON::TYPE type = TranslateType(params[0]);
    if (CAddonMgr::Get().GetDefault(type, addon))
    {
      CGUIDialogAddonSettings::ShowAndGetInput(addon);
      if (type == ADDON_VIZ)
        g_windowManager.SendMessage(GUI_MSG_VISUALISATION_RELOAD, 0, 0);
    }
  }
  else if (execute.Equals("addon.default.set") && params.size() == 1)
  {
    CStdString addonID;
    TYPE type = TranslateType(params[0]);
    bool allowNone = false;
    if (type == ADDON_VIZ)
      allowNone = true;

    if (type != ADDON_UNKNOWN && 
        CGUIWindowAddonBrowser::SelectAddonID(type,addonID,allowNone))
    {
      CAddonMgr::Get().SetDefault(type,addonID);
      if (type == ADDON_VIZ)
        g_windowManager.SendMessage(GUI_MSG_VISUALISATION_RELOAD, 0, 0);
    }
  }
  else if (execute.Equals("addon.opensettings") && params.size() == 1)
  {
    AddonPtr addon;
    if (CAddonMgr::Get().GetAddon(params[0], addon))
      CGUIDialogAddonSettings::ShowAndGetInput(addon);
  }
  else if (execute.Equals("updateaddonrepos"))
  {
    CAddonInstaller::Get().UpdateRepos(true);
  }
  else if (execute.Equals("updatelocaladdons"))
  {
    CAddonMgr::Get().FindAddons();
  }
  else if (execute.Equals("toggledpms"))
  {
    g_application.ToggleDPMS(true);
  }
  else if (execute.Equals("cectogglestate"))
  {
    CApplicationMessenger::Get().CECToggleState();
  }
  else if (execute.Equals("cecactivatesource"))
  {
    CApplicationMessenger::Get().CECActivateSource();
  }
  else if (execute.Equals("cecstandby"))
  {
    CApplicationMessenger::Get().CECStandby();
  }
#if defined(HAS_LIRC) || defined(HAS_IRSERVERSUITE)
  else if (execute.Equals("lirc.stop"))
  {
    g_RemoteControl.Disconnect();
    g_RemoteControl.setUsed(false);
  }
  else if (execute.Equals("lirc.start"))
  {
    g_RemoteControl.setUsed(true);
    g_RemoteControl.Initialize();
  }
  else if (execute.Equals("lirc.send"))
  {
    CStdString command;
    for (int i = 0; i < (int)params.size(); i++)
    {
      command += params[i];
      if (i < (int)params.size() - 1)
        command += ' ';
    }
    g_RemoteControl.AddSendCommand(command);
  }
#endif
  else if (execute.Equals("weather.locationset"))
  {
    int loc = atoi(params[0]);
    CGUIMessage msg(GUI_MSG_ITEM_SELECT, 0, 0, loc);
    g_windowManager.SendMessage(msg, WINDOW_WEATHER);
  }
  else if (execute.Equals("weather.locationnext"))
  {
    CGUIMessage msg(GUI_MSG_MOVE_OFFSET, 0, 0, 1);
    g_windowManager.SendMessage(msg, WINDOW_WEATHER);
  }
  else if (execute.Equals("weather.locationprevious"))
  {
    CGUIMessage msg(GUI_MSG_MOVE_OFFSET, 0, 0, -1);
    g_windowManager.SendMessage(msg, WINDOW_WEATHER);
  }
  else if (execute.Equals("weather.refresh"))
  {
    CGUIMessage msg(GUI_MSG_MOVE_OFFSET, 0, 0, 0);
    g_windowManager.SendMessage(msg, WINDOW_WEATHER);
  }
  else if (execute.Equals("videolibrary.search"))
  {
    CGUIMessage msg(GUI_MSG_SEARCH, 0, 0, 0);
    g_windowManager.SendMessage(msg, WINDOW_VIDEO_NAV);
  }
  else if (execute.Equals("toggledebug"))
  {
    bool debug = CSettings::Get().GetBool("debug.showloginfo");
    CSettings::Get().SetBool("debug.showloginfo", !debug);
    g_advancedSettings.SetDebugMode(!debug);
  }
  else if (execute.Equals("startpvrmanager"))
  {
    g_application.StartPVRManager();
  }
  else if (execute.Equals("stoppvrmanager"))
  {
    g_application.StopPVRManager();
  }
  else if (execute.Equals("StartAndroidActivity") && params.size() > 0)
  {
    CApplicationMessenger::Get().StartAndroidActivity(params);
  }
  else if (execute.Equals("SetStereoMode") && !parameter.empty())
  {
    CAction action = CStereoscopicsManager::Get().ConvertActionCommandToAction(execute, parameter);
    if (action.GetID() != ACTION_NONE)
      CApplicationMessenger::Get().SendAction(action);
    else
    {
      CLog::Log(LOGERROR,"Builtin 'SetStereoMode' called with unknown parameter: %s", parameter.c_str());
      return -2;
    }
  }
  else
    return -1;
  return 0;
}
