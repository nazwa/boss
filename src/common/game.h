/*	BOSS

	A "one-click" program for users that quickly optimises and avoids
	detrimental conflicts in their TES IV: Oblivion, Nehrim - At Fate's Edge,
	TES V: Skyrim, Fallout 3 and Fallout: New Vegas mod load orders.

	Copyright (C) 2009-2012    BOSS Development Team.

	This file is part of BOSS.

	BOSS is free software: you can redistribute
	it and/or modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation, either version 3 of
	the License, or (at your option) any later version.

	BOSS is distributed in the hope that it will
	be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with BOSS.  If not, see
	<http://www.gnu.org/licenses/>.

	$Revision: 3135 $, $Date: 2011-08-17 22:01:17 +0100 (Wed, 17 Aug 2011) $
*/

#ifndef COMMON_GAME_H_
#define COMMON_GAME_H_

#include <cstdint>

#include <string>
#include <vector>

#include <boost/filesystem.hpp>

#include "common/dll_def.h"
#include "common/item_list.h"
#include "common/rule_line.h"
#include "output/boss_log.h"

namespace boss {

class BOSS_COMMON Item;
class Version;

// MCP Note: Possibly convert these to enums?
// The following are for signifying what load order method is being used:
BOSS_COMMON extern const std::uint32_t LOMETHOD_TIMESTAMP;
BOSS_COMMON extern const std::uint32_t LOMETHOD_TEXTFILE;

BOSS_COMMON std::uint32_t DetectGame(std::vector<std::uint32_t> &detectedGames,
                                     std::vector<std::uint32_t> &undetectedGames);  // Throws exception if error.

class BOSS_COMMON Game {  // Constructor depends on gl_update_only.
 public:
	Game();  // Sets game to AUTODETECT, with all other vars being empty.
	Game(const std::uint32_t gameCode, const std::string path = "",
	     const bool noPathInit = false);  // Empty path means constructor will detect its location. If noPathInit is true, then the data, active plugins list and loadorder.txt paths will not be set, and the game's BOSS subfolder will not be created.

	bool IsInstalled() const;
	bool IsInstalledLocally() const;

	std::uint32_t Id() const;
	std::string Name() const;  // Returns the game's name, eg. "TES IV: Oblivion".
	std::string ScriptExtender() const;
	Item MasterFile() const;  // Returns the game's master file. To get its timestamp, use .GetModTime() on it.

	Version GetVersion() const;
	std::uint32_t GetLoadOrderMethod() const;

	boost::filesystem::path Executable() const;
	boost::filesystem::path GameFolder() const;
	boost::filesystem::path DataFolder() const;
	boost::filesystem::path SEPluginsFolder() const;
	boost::filesystem::path SEExecutable() const;
	boost::filesystem::path ActivePluginsFile() const;
	boost::filesystem::path LoadOrderFile() const;
	boost::filesystem::path Masterlist() const;
	boost::filesystem::path Userlist() const;
	boost::filesystem::path Modlist() const;
	boost::filesystem::path OldModlist() const;
	boost::filesystem::path Log(std::uint32_t format) const;

	// Creates directory in BOSS folder for BOSS's game-specific files.
	void CreateBOSSGameFolder();

	// Apply the positioning of plugins in the masterlist to the modlist, putting unrecognised plugins after recognised plugins in their original order. Alters modlist.
	void ApplyMasterlist();

	// Apply any user rules to the modlist. Alters modlist and bosslog.
	void ApplyUserlist();

	// Scans the data folder for script extender plugins and outputs their info to the bosslog. Alters bosslog.
	void ScanSEPlugins();

	// Sorts the plugins in the data folder, changing timestamps or plugins.txt/loadorder.txt as required. Alters bosslog.
	void SortPlugins();

	ItemList modlist;
	ItemList masterlist;
	RuleList userlist;
	BossLog bosslog;

 private:
	// Can be used to get the location of the LOCALAPPDATA folder (and its Windows XP equivalent).
	boost::filesystem::path GetLocalAppDataPath();

	std::uint32_t id;
	std::uint32_t loMethod;
	std::string name;

	std::string executable;
	std::string masterFile;
	std::string scriptExtender;
	std::string seExecutable;

	std::string registryKey;
	std::string registrySubKey;

	std::string bossFolderName;
	std::string appdataFolderName;
	std::string pluginsFolderName;
	std::string pluginsFileName;

	boost::filesystem::path gamePath;       // Path to the game's folder.
	boost::filesystem::path pluginsPath;    // Path to the file in which active plugins are listed.
	boost::filesystem::path loadorderPath;  // Path to the file which lists total load order.
};

}  // namespace boss
#endif  // COMMON_GAME_H_
