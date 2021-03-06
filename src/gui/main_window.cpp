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

	$Revision: 2188 $, $Date: 2011-01-20 10:05:16 +0000 (Thu, 20 Jan 2011) $
*/

// We want to ensure that the GUI-specific code in BOSS-Common is included.

#include "gui/main_window.h"

// TODO(MCP): Trim down the includes by removing the unneeded ones.

#include <clocale>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <ctime>

#include <algorithm>
#include <exception>
#include <iostream>
#include <locale>
#include <string>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <boost/exception/get_error_info.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/detail/utf8_codecvt_facet.hpp>
#include <boost/format.hpp>
#include <boost/locale.hpp>

#include <wx/aboutdlg.h>
#include <wx/gdicmn.h>
#include <wx/snglinst.h>

#include "common/error.h"
#include "common/game.h"
#include "common/globals.h"
#include "common/rule_line.h"
#include "common/settings.h"
#include "gui/boss_icon.xpm"
#include "gui/element_ids.h"
#include "gui/settings_window.h"
#include "gui/user_rule_editor.h"
#include "output/boss_log.h"
#include "output/output.h"
#include "support/logger.h"
#include "updating/updater.h"

wxDEFINE_EVENT(wxEVT_COMMAND_MYTHREAD_UPDATE, wxThreadEvent);

using boss::MainFrame;

wxBEGIN_EVENT_TABLE(MainFrame, wxFrame)
	EVT_CLOSE(MainFrame::OnClose)
	EVT_MENU(MENU_Quit, MainFrame::OnQuit)
	EVT_MENU(OPTION_EditUserRules, MainFrame::OnEditUserRules)
	EVT_MENU(OPTION_OpenBOSSlog, MainFrame::OnOpenFile)
	EVT_MENU(OPTION_Run, MainFrame::OnRunBOSS)
	EVT_MENU(MENU_OpenMainReadMe, MainFrame::OnOpenFile)
	EVT_MENU(MENU_OpenUserlistReadMe, MainFrame::OnOpenFile)
	EVT_MENU(MENU_OpenMasterlistReadMe, MainFrame::OnOpenFile)
	EVT_MENU(MENU_OpenVersionHistory, MainFrame::OnOpenFile)
	EVT_MENU(MENU_OpenLicenses, MainFrame::OnOpenFile)
	EVT_MENU(MENU_ShowAbout, MainFrame::OnAbout)
	EVT_MENU(MENU_ShowSettings, MainFrame::OnOpenSettings)
	EVT_MENU(MENU_Oblivion, MainFrame::OnGameChange)
	EVT_MENU(MENU_Nehrim, MainFrame::OnGameChange)
	EVT_MENU(MENU_Skyrim, MainFrame::OnGameChange)
	EVT_MENU(MENU_Fallout3, MainFrame::OnGameChange)
	EVT_MENU(MENU_FalloutNewVegas, MainFrame::OnGameChange)
	EVT_BUTTON(OPTION_Run, MainFrame::OnRunBOSS)
	EVT_BUTTON(OPTION_EditUserRules, MainFrame::OnEditUserRules)
	EVT_BUTTON(OPTION_OpenBOSSlog, MainFrame::OnOpenFile)
	EVT_CHOICE(DROPDOWN_LogFormat, MainFrame::OnFormatChange)
	EVT_CHOICE(DROPDOWN_Game, MainFrame::OnGameChange)
	EVT_CHOICE(DROPDOWN_Revert, MainFrame::OnRevertChange)
	EVT_CHECKBOX(CHECKBOX_ShowBOSSlog, MainFrame::OnLogDisplayChange)
	EVT_CHECKBOX(CHECKBOX_Update, MainFrame::OnUpdateChange)
	EVT_CHECKBOX(CHECKBOX_EnableCRCs, MainFrame::OnCRCDisplayChange)
	EVT_CHECKBOX(CHECKBOX_TrialRun, MainFrame::OnTrialRunChange)
	EVT_RADIOBUTTON(RADIOBUTTON_SortOption, MainFrame::OnRunTypeChange)
	EVT_RADIOBUTTON(RADIOBUTTON_UpdateOption, MainFrame::OnRunTypeChange)
	EVT_RADIOBUTTON(RADIOBUTTON_UndoOption, MainFrame::OnRunTypeChange)
wxEND_EVENT_TABLE()

wxIMPLEMENT_APP(boss::BossGUI);

namespace boss {

namespace fs = boost::filesystem;
namespace bloc = boost::locale;  // MCP Note: Will this interfere with std::locale loc?

// Draws the main window when program starts.
bool BossGUI::OnInit() {
	Settings ini;
	// Set up variable defaults.
	if (fs::exists(ini_path)) {
		try {
			ini.Load(ini_path);
		} catch (boss_error &e) {
			LOG_ERROR("Error: %s", e.getString().c_str());
			wxMessageBox(FromUTF8(boost::format(bloc::translate("Error: %1% Details: %2%")) % e.getString() % Outputter(PLAINTEXT, ini.ErrorBuffer()).AsString()),
			             translate("BOSS: Error"),
			             wxOK | wxICON_ERROR,
			             NULL);
		}
	}

	if (gl_debug_verbosity > 0)
		g_logger.setStream(debug_log_path.string().c_str());
	// It's ok if this number is too high, setVerbosity will handle it
	g_logger.setVerbosity(static_cast<LogVerbosity>(LV_WARN + gl_debug_verbosity));

	// Specify location of language dictionaries
	bloc::generator gen;
	gen.add_messages_path(l10n_path.string());
	gen.add_messages_domain("boss");

	// Set the locale to get encoding and language conversions working correctly.
	std::string localeId = "";
	wxLanguage lang;
	// TODO(MCP): Look at converting this to a switch-statement
	if (gl_language == ENGLISH) {
		localeId = "en.UTF-8";
		lang = wxLANGUAGE_ENGLISH;
	} else if (gl_language == SPANISH) {
		localeId = "es.UTF-8";
		lang = wxLANGUAGE_SPANISH;
	} else if (gl_language == GERMAN) {
		localeId = "de.UTF-8";
		lang = wxLANGUAGE_GERMAN;
	} else if (gl_language == RUSSIAN) {
		localeId = "ru.UTF-8";
		lang = wxLANGUAGE_RUSSIAN;
	} else if (gl_language == SIMPCHINESE) {
		localeId = "zh_CN.UTF-8";
		lang = wxLANGUAGE_CHINESE_SIMPLIFIED;
	}

	try {
		std::locale::global(gen(localeId));  // MCP Note: Is #include <locale> needed for this?
		std::cout.imbue(std::locale());
		// Need to also set up wxWidgets locale so that its default interface text comes out in the right language.
		wxLoc = new wxLocale();
		if (!wxLoc->Init(lang, wxLOCALE_LOAD_DEFAULT))
			//throw std::exception("System GUI text could not be set.");  // MCP Note: Is this boost::exception or std::exception?
			throw boss_error(BOSS_ERROR_GUI_WINDOW_INIT_FAIL);
		wxLocale::AddCatalogLookupPathPrefix(l10n_path.string());
		wxLoc->AddCatalog("wxstd");
	} catch(std::exception &e) {  // MCP Note: Is this std::exception or boost::exception?
		LOG_ERROR("could not implement translation: %s", e.what());
		wxMessageBox(FromUTF8(boost::format(bloc::translate("Error: could not apply translation: %1%")) % e.what()),
		             translate("BOSS: Error"),
		             wxOK | wxICON_ERROR,
		             NULL);
	}
	std::locale global_loc = std::locale();
	std::locale loc(global_loc, new fs::detail::utf8_codecvt_facet());
	fs::path::imbue(loc);


	// Check if GUI is already running.
	wxSingleInstanceChecker *checker = new wxSingleInstanceChecker;

	if (checker->IsAnotherRunning()) {
		wxMessageBox(translate("Error: The BOSS GUI is already running. This instance will now quit."),
		             translate("BOSS: Error"),
		             wxOK | wxICON_ERROR,
		             NULL);

		delete checker;  // OnExit() won't be called if we return false
		checker = NULL;

		return false;
	}

	MainFrame *frame = new MainFrame(wxT("BOSS"));

	LOG_DEBUG("Detecting game...");

	Game game;
	std::vector<std::uint32_t> detected;
	try {
		std::vector<std::uint32_t> undetected;
		std::uint32_t detectedGame = DetectGame(detected, undetected);
		if (detectedGame == AUTODETECT) {
			wxArrayString choices;

			for (std::size_t i = 0, max = detected.size(); i < max; i++)
				choices.Add(Game(detected[i], "", true).Name());  // Don't need to convert name, known to be only ASCII chars.
			for (std::size_t i = 0, max = undetected.size(); i < max; i++)
				choices.Add(Game(undetected[i], "", true).Name() + translate(" (not detected)"));  // Don't need to convert name, known to be only ASCII chars.

			std::size_t ans;

			wxSingleChoiceDialog *choiceDia = new wxSingleChoiceDialog(
			    frame,
			    translate("Please pick which game to run BOSS for:"),
			    translate("BOSS: Select Game"),
			    choices);
			//choiceDia->SetIcon(wxIconLocation("BOSS.ico"));
			choiceDia->SetIcon(wxICON(boss_icon));

			if (choiceDia->ShowModal() != wxID_OK)
				throw boss_error(BOSS_ERROR_NO_GAME_DETECTED);

			ans = choiceDia->GetSelection();
			choiceDia->Close(true);

			if (ans < detected.size())
				detectedGame = detected[ans];
			else if (ans < detected.size() + undetected.size())
				detectedGame = undetected[ans - detected.size()];
			else
				throw boss_error(BOSS_ERROR_NO_GAME_DETECTED);
		}
		game = Game(detectedGame);
		game.CreateBOSSGameFolder();
		LOG_INFO("Game detected: %s", game.Name().c_str());
	} catch (boss_error /*&e*/) {
		return false;
	}
	frame->SetGames(game, detected);

	//frame->SetIcon(wxIconLocation("BOSS.ico"));
	frame->SetIcon(wxICON(boss_icon));
	frame->Show();  // MCP Note: Show apparently takes a bool and defaults to true, so replace TRUE with either that or remove it? Not sure with TRUE comes from...
	SetTopWindow(frame);

	return true;
}

MainFrame::MainFrame(const wxChar *title) : wxFrame(NULL, wxID_ANY, title, wxDefaultPosition, wxDefaultSize, wxMINIMIZE_BOX|wxSYSTEM_MENU|wxCAPTION|wxCLOSE_BOX|wxCLIP_CHILDREN) {
	// Some variable setup.
	isStartup = true;

	wxString BOSSlogFormat[] = {wxT("HTML"), translate("Plain Text")};

	wxString UndoLevel[] = {translate("Last Run"),
	                        translate("2nd Last Run")};

	// Set up menu bar first.
	MenuBar = new wxMenuBar();
	// File Menu
	FileMenu = new wxMenu();
	FileMenu->Append(OPTION_OpenBOSSlog,
	                 translate("&View BOSS Log"),
	                 translate("Opens your BOSSlog."));
	FileMenu->Append(OPTION_Run,
	                 translate("&Run BOSS"),
	                 translate("Runs BOSS with the options you have chosen."));
	FileMenu->AppendSeparator();
	FileMenu->Append(MENU_Quit,
	                 translate("&Quit"),
	                 translate("Quit BOSS."));
	MenuBar->Append(FileMenu, translate("&File"));
	// Edit Menu
	EditMenu = new wxMenu();
	EditMenu->Append(OPTION_EditUserRules,
	                 translate("&User Rules..."),
	                 translate("Opens your userlist in your default text editor."));
	EditMenu->Append(MENU_ShowSettings,
	                 translate("&Settings..."),
	                 translate("Opens the Settings window."));
	MenuBar->Append(EditMenu, translate("&Edit"));
	// Game menu
	GameMenu = new wxMenu();
	GameMenu->AppendRadioItem(MENU_Oblivion, wxT("&Oblivion"),
	                          translate("Switch to running BOSS for Oblivion."));
	GameMenu->AppendRadioItem(MENU_Nehrim, wxT("&Nehrim"),
	                          translate("Switch to running BOSS for Nehrim."));
	GameMenu->AppendRadioItem(MENU_Skyrim, wxT("&Skyrim"),
	                          translate("Switch to running BOSS for Skyrim."));
	GameMenu->AppendRadioItem(MENU_Fallout3, wxT("&Fallout 3"),
	                          translate("Switch to running BOSS for Fallout 3."));
	GameMenu->AppendRadioItem(MENU_FalloutNewVegas, wxT("&Fallout: New Vegas"),
	                          translate("Switch to running BOSS for Fallout: New Vegas."));
	MenuBar->Append(GameMenu, translate("&Active Game"));
	// About menu
	HelpMenu = new wxMenu();
	HelpMenu->Append(MENU_OpenMainReadMe,
	                 translate("Open &Main Readme"),
	                 translate("Opens the main BOSS readme in your default web browser."));
	HelpMenu->Append(MENU_OpenUserlistReadMe,
	                 translate("Open &Userlist Syntax Doc"),
	                 translate("Opens the BOSS userlist syntax documentation in your default web browser."));
	HelpMenu->Append(MENU_OpenMasterlistReadMe,
	                 translate("Open &Masterlist Syntax Doc"),
	                 translate("Opens the BOSS masterlist syntax documentation in your default web browser."));
	HelpMenu->Append(MENU_OpenVersionHistory,
	                 translate("Open &Version History"),
	                 translate("Opens the BOSS version history in your default web browser."));
	HelpMenu->Append(MENU_OpenLicenses,
	                 translate("View &Copyright Licenses"),
	                 translate("View the GNU General Public License v3.0 and GNU Free Documentation License v1.3."));
	HelpMenu->AppendSeparator();
	HelpMenu->Append(MENU_ShowAbout,
	                 translate("&About BOSS..."),
	                 translate("Shows information about BOSS."));
	MenuBar->Append(HelpMenu, translate("&Help"));
	SetMenuBar(MenuBar);

	// Set up stuff in the frame.
	// MCP Note: Change the background color of the windows?
	SetBackgroundColour(wxColour(255, 255, 255));

	// Contents in one big resizing box.
	wxBoxSizer *bigBox = new wxBoxSizer(wxHORIZONTAL);

	// Create first column box and add the output options to it.
	wxBoxSizer *columnBox = new wxBoxSizer(wxVERTICAL);
	wxStaticBoxSizer *outputOptionsBox = new wxStaticBoxSizer(wxVERTICAL,
	                                                          this,
	                                                          translate("Output Options"));
	wxBoxSizer *formatBox = new wxBoxSizer(wxHORIZONTAL);

	// MCP Note: Maybe separate out the variable declarations so that they're not inside of a function call?
	// Add stuff to output options sizer.
	outputOptionsBox->Add(ShowLogBox = new wxCheckBox(this, CHECKBOX_ShowBOSSlog, translate("Show BOSS Log On Completion")), 0, wxALL, 5);
	outputOptionsBox->Add(CRCBox = new wxCheckBox(this, CHECKBOX_EnableCRCs, translate("Display File CRCs")), 0, wxLEFT | wxBOTTOM, 5);
	formatBox->Add(new wxStaticText(this, wxID_ANY, translate("BOSS Log Format: ")), 1, wxLEFT | wxBOTTOM, 5);
	formatBox->Add(FormatChoice = new wxChoice(this, DROPDOWN_LogFormat, wxPoint(110, 60), wxDefaultSize, 2, BOSSlogFormat, wxCB_READONLY), 0, wxALIGN_RIGHT | wxRIGHT | wxBOTTOM, 5);
	// Add the verbosityBox to its parent now to preserve layout.
	outputOptionsBox->Add(formatBox, 0, wxEXPAND, 0);
	columnBox->Add(outputOptionsBox, 0, wxBOTTOM, 20);

	// Now add the main buttons to the first column.
	wxBoxSizer *buttonBox = new wxBoxSizer(wxVERTICAL);
	buttonBox->Add(EditUserRulesButton = new wxButton(this, OPTION_EditUserRules, translate("Edit User Rules")), 0, wxALIGN_CENTRE|wxBOTTOM, 5);
	buttonBox->Add(RunBOSSButton = new wxButton(this, OPTION_Run, translate("Run BOSS")), 0, wxALIGN_CENTRE);
	buttonBox->Add(OpenBOSSlogButton = new wxButton(this, OPTION_OpenBOSSlog, translate("View BOSS Log")), 0, wxALIGN_CENTRE|wxTOP, 5);
	columnBox->Add(buttonBox, 0, wxALIGN_CENTER, 20);

	// Add the first column to the big box.
	bigBox->Add(columnBox, 0, wxALL, 20);

	// The second column has a border.
	wxStaticBoxSizer *runOptionsBox = new wxStaticBoxSizer(wxVERTICAL,
	                                                       this,
	                                                       translate("Run Options"));
	wxBoxSizer *sortBox = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer *undoBox = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer *revertBox = new wxBoxSizer(wxHORIZONTAL);

	// Run Options
	runOptionsBox->Add(SortOption = new wxRadioButton(this, RADIOBUTTON_SortOption, translate("Sort Plugins")), 0, wxALL, 5);

	// Sort option stuff.
	sortBox->Add(UpdateBox = new wxCheckBox(this, CHECKBOX_Update, translate("Update Masterlist")), 0, wxBOTTOM, 5);
	sortBox->Add(TrialRunBox = new wxCheckBox(this, CHECKBOX_TrialRun, translate("Perform Trial Run")));
	runOptionsBox->Add(sortBox, 0, wxLEFT | wxRIGHT, 20);
	runOptionsBox->AddSpacer(10);

	// Update only stuff.
	runOptionsBox->Add(UpdateOption = new wxRadioButton(this, RADIOBUTTON_UpdateOption, translate("Update Masterlist Only")), 0, wxALL, 5);
	runOptionsBox->AddSpacer(10);

	// Undo option stuff.
	runOptionsBox->Add(UndoOption = new wxRadioButton(this, RADIOBUTTON_UndoOption, translate("Undo Changes")), 0, wxALL, 5);
	revertBox->Add(RevertText = new wxStaticText(this, wxID_ANY, translate("Undo Level: ")));
	revertBox->Add(RevertChoice = new wxChoice(this, DROPDOWN_Revert, wxDefaultPosition, wxDefaultSize, 2, UndoLevel));
	runOptionsBox->Add(revertBox, 0, wxLEFT | wxRIGHT, 20);
	runOptionsBox->AddSpacer(5);

	bigBox->Add(runOptionsBox, 0, wxTOP | wxRIGHT | wxBOTTOM, 20);


	// Tooltips
	FormatChoice->SetToolTip(translate("This decides both the format of BOSSlog generated when you click the \"Run BOSS\" button and the BOSSlog format opened when you click the \"View BOSSlog\" button."));
	OpenBOSSlogButton->SetToolTip(translate("The format of BOSSlog this opens is decided by the setting of the \"BOSSlog Format\" Output Option above."));
	TrialRunBox->SetToolTip(translate("Runs BOSS, simulating its changes to your load order, but doesn't actually reorder your mods."));

	// Set option values based on initialised variable values.
	RunBOSSButton->SetDefault();

	if (!gl_silent)
		ShowLogBox->SetValue(true);

	if (gl_log_format == HTML)
		FormatChoice->SetSelection(0);
	else
		FormatChoice->SetSelection(1);

	if (gl_show_CRCs)
		CRCBox->SetValue(true);

	if (gl_update)
		UpdateBox->SetValue(true);

	if (gl_trial_run)
		TrialRunBox->SetValue(true);

	if (gl_revert < 2)
		RevertChoice->SetSelection(0);
	else
		RevertChoice->SetSelection(1);

	if (gl_revert == 0 && !gl_update_only) {
		SortOption->SetValue(true);
		UpdateBox->Enable(true);
		TrialRunBox->Enable(true);

		RevertText->Enable(false);
		RevertChoice->Enable(false);
	} else if (gl_update_only) {
		UpdateOption->SetValue(true);

		UpdateBox->Enable(false);
		TrialRunBox->Enable(false);
		RevertText->Enable(false);
		RevertChoice->Enable(false);
	} else if (gl_revert > 0) {
		UndoOption->SetValue(true);
		RevertText->Enable(true);
		RevertChoice->Enable(true);

		UpdateBox->Enable(false);
		TrialRunBox->Enable(false);
	}

	// Now set up the status bar.
	CreateStatusBar(1);
	SetStatusText(translate("Ready"));

	// Now set the layout and sizes.
	SetSizerAndFit(bigBox);
}

void MainFrame::OnOpenSettings(wxCommandEvent &event) {
	// Tell the user that stuff is happenining.
	SettingsFrame *settings = new SettingsFrame(translate("BOSS: Settings"),
	                                            this);
	//settings->SetIcon(wxIconLocation("BOSS.ico"));
	settings->SetIcon(wxICON(boss_icon));
	settings->Show();
}

// Called when program exits.
void MainFrame::OnQuit(wxCommandEvent &event) {
	Close(true);  // Tells the OS to quit running this process
}

void MainFrame::OnRunBOSS(wxCommandEvent &event) {
	fs::path sortfile;  // Modlist/masterlist to sort plugins using.

	// Tell the user that stuff is happenining.
	wxProgressDialog *progDia = new wxProgressDialog(translate("BOSS: Working..."),
	                                                 translate("BOSS working..."),
	                                                 1000,
	                                                 this,
	                                                 wxPD_APP_MODAL|wxPD_AUTO_HIDE|wxPD_ELAPSED_TIME|wxPD_CAN_ABORT);

	LOG_INFO("BOSS starting...");

	// Clear modlist, masterlist and userlist in case they were filled by the User Rules Manager but the user has made changes to their game.
	game.modlist.Clear();
	game.masterlist.Clear();
	game.userlist.Clear();
	game.bosslog.Clear();

	// Set format and some vars for BOSS Log.
	game.bosslog.SetFormat(gl_log_format);
	game.bosslog.scriptExtender = game.ScriptExtender();
	game.bosslog.gameName = game.Name();


	/////////////////////////////////////////////////////////
	// Update Masterlist
	/////////////////////////////////////////////////////////

	if (gl_revert < 1 && (gl_update || gl_update_only)) {
		progDia->Update(0,
		                translate("Updating to the latest masterlist from the online repository..."));
		LOG_DEBUG("Updating masterlist...");
		try {
			std::string revision = UpdateMasterlist(game, progress, progDia);
			std::string message = (boost::format(translate("Masterlist updated; at revision: %1%.")) % revision).str();
			game.bosslog.updaterOutput << LIST_ITEM_CLASS_SUCCESS << message;
		}
		catch (boss_error &e) {
			game.bosslog.updaterOutput << LIST_ITEM_CLASS_ERROR << translate("Error: masterlist update failed.").ToStdString() << LINE_BREAK
			                           << (boost::format(translate("Details: %1%")) % e.getString()).str() << LINE_BREAK;
			LOG_ERROR("Error: masterlist update failed. Details: %s",
			          e.getString().c_str());
		}
	} else {
		std::string revision = GetMasterlistVersion(game);
		std::string message = (boost::format(translate("Masterlist updating disabled; at revision: %1%.")) % revision).str();
		game.bosslog.updaterOutput << LIST_ITEM_CLASS_SUCCESS << message;
	}

	// If true, exit BOSS now. Flush earlyBOSSlogBuffer to the bosslog and exit.
	if (gl_update_only == true) {
		try {
			game.bosslog.Save(game.Log(gl_log_format), true);
		} catch (boss_error &e) {
			LOG_ERROR("Critical Error: %s", e.getString().c_str());
		}
		if (!gl_silent)
			wxLaunchDefaultApplication(game.Log(gl_log_format).string());  // Displays the BOSSlog.
		progDia->Destroy();
		if (gl_close_gui_after_sorting)
			this->Close(true);
		return;
	}

	progDia->Pulse(translate("BOSS working..."));
	if (progDia->WasCancelled()) {
		progDia->Destroy();
		if (gl_close_gui_after_sorting)
			this->Close(true);
		return;
	}

	///////////////////////////////////
	// Resume Error Condition Checks
	///////////////////////////////////

	// Build and save modlist.
	try {
		game.modlist.Load(game, game.DataFolder());
		if (gl_revert < 1)
			game.modlist.Save(game.Modlist(), game.OldModlist());
	} catch (boss_error &e) {
		LOG_ERROR("Failed to load/save modlist, error was: %s",
		          e.getString().c_str());
		game.bosslog.criticalError << LIST_ITEM_CLASS_ERROR << (boost::format(bloc::translate("Critical Error: %1%")) % e.getString()).str() << LINE_BREAK
		                           << bloc::translate("Check the Troubleshooting section of the ReadMe for more information and possible solutions.") << LINE_BREAK
		                           << bloc::translate("Utility will end now.");
		try {
			game.bosslog.Save(game.Log(gl_log_format), true);
		} catch (boss_error &e) {
			LOG_ERROR("Critical Error: %s", e.getString().c_str());
		}
		if (!gl_silent)
			wxLaunchDefaultApplication(game.Log(gl_log_format).string());  // Displays the BOSSlog.txt.
		progDia->Destroy();
		if (gl_close_gui_after_sorting)
			this->Close(true);
		return;  // Fail in screaming heap.
	}

	progDia->Pulse();
	if (progDia->WasCancelled()) {
		progDia->Destroy();
		if (gl_close_gui_after_sorting)
			this->Close(true);
		return;
	}

	/////////////////////////////////
	// Parse Master- and Userlists
	/////////////////////////////////
	// Masterlist parse errors are critical, ini and userlist parse errors are not.


	// Set masterlist path to be used.
	if (gl_revert == 1)
		sortfile = game.Modlist();
	else if (gl_revert == 2)
		sortfile = game.OldModlist();
	else
		sortfile = game.Masterlist();
	LOG_INFO("Using sorting file: %s", sortfile.string().c_str());

	// Parse masterlist/modlist backup into data structure.
	try {
		LOG_INFO("Starting to parse sorting file: %s",
		         sortfile.string().c_str());
		game.masterlist.Load(game, sortfile);
		LOG_INFO("Starting to parse conditionals from sorting file: %s",
		         sortfile.string().c_str());
		game.masterlist.EvalConditions(game);
		game.masterlist.EvalRegex(game);
		game.bosslog.globalMessages = game.masterlist.GlobalMessageBuffer();
		game.bosslog.parsingErrors.push_back(game.masterlist.ErrorBuffer());
	} catch (boss_error &e) {
		LOG_ERROR("Critical Error: %s", e.getString().c_str());
		if (e.getCode() == BOSS_ERROR_FILE_PARSE_FAIL) {
			game.bosslog.criticalError << game.masterlist.ErrorBuffer();
		} else if (e.getCode() == BOSS_ERROR_CONDITION_EVAL_FAIL) {
			game.bosslog.criticalError << LIST_ITEM_CLASS_ERROR << e.getString();
		} else {
			game.bosslog.criticalError << LIST_ITEM_CLASS_ERROR << (boost::format(bloc::translate("Critical Error: %1%")) % e.getString()).str() << LINE_BREAK
			                           << bloc::translate("Check the Troubleshooting section of the ReadMe for more information and possible solutions.") << LINE_BREAK
			                           << bloc::translate("Utility will end now.");
		}
		try {
			game.bosslog.Save(game.Log(gl_log_format), true);
		} catch (boss_error &e) {
			LOG_ERROR("Critical Error: %s", e.getString().c_str());
		}
		if (!gl_silent)
			wxLaunchDefaultApplication(game.Log(gl_log_format).string());  // Displays the BOSSlog.txt.
		progDia->Destroy();
		if (gl_close_gui_after_sorting)
			this->Close(true);
		return;  // Fail in screaming heap.
	}

	LOG_INFO("Starting to parse userlist.");
	try {
		game.userlist.Load(game, game.Userlist());
		std::vector<ParsingError> errs = game.userlist.ErrorBuffer();
		game.bosslog.parsingErrors.insert(game.bosslog.parsingErrors.end(),
		                                  errs.begin(), errs.end());
	} catch (boss_error &e) {
		std::vector<ParsingError> errs = game.userlist.ErrorBuffer();
		game.bosslog.parsingErrors.insert(game.bosslog.parsingErrors.end(),
		                                  errs.begin(), errs.end());
		game.userlist.Clear();  // If userlist has parsing errors, empty it so no rules are applied.
		LOG_ERROR("Error: %s", e.getString().c_str());
	}

	progDia->Pulse();
	if (progDia->WasCancelled()) {
		progDia->Destroy();
		if (gl_close_gui_after_sorting)
			this->Close(true);
		return;
	}

	/////////////////////////////////////////////////
	// Perform Sorting Functionality
	/////////////////////////////////////////////////

	try {
		game.ApplyMasterlist();
		LOG_INFO("masterlist now filled with ordered mods and modlist filled with unknowns.");
		game.ApplyUserlist();
		LOG_INFO("userlist sorting process finished.");
		game.ScanSEPlugins();
		game.SortPlugins();
		game.bosslog.Save(game.Log(gl_log_format), true);
	} catch (boss_error &e) {
		LOG_ERROR("Critical Error: %s", e.getString().c_str());
		game.bosslog.criticalError << LIST_ITEM_CLASS_ERROR << (boost::format(bloc::translate("Critical Error: %1%")) % e.getString()).str() << LINE_BREAK
		                           << bloc::translate("Check the Troubleshooting section of the ReadMe for more information and possible solutions.") << LINE_BREAK
		                           << bloc::translate("Utility will end now.");
		try {
			game.bosslog.Save(game.Log(gl_log_format), true);
		} catch (boss_error &e) {
			LOG_ERROR("Critical Error: %s", e.getString().c_str());
		}
		if (!gl_silent)
			wxLaunchDefaultApplication(game.Log(gl_log_format).string());  // Displays the BOSSlog.txt.
		progDia->Destroy();
		if (gl_close_gui_after_sorting)
			this->Close(true);
		return;  // Fail in screaming heap.
	}

	LOG_INFO("Launching boss log in browser.");
	if (!gl_silent)
		wxLaunchDefaultApplication(game.Log(gl_log_format).string());  // Displays the BOSSlog.txt.
	LOG_INFO("BOSS finished.");
	progDia->Destroy();
	if (gl_close_gui_after_sorting)
		this->Close(true);
}

// Call when a file is opened. Either readmes or BOSS Logs.
void MainFrame::OnOpenFile(wxCommandEvent &event) {
	if (event.GetId() == OPTION_OpenBOSSlog) {
		if (fs::exists(game.Log(gl_log_format))) {
			wxLaunchDefaultApplication(game.Log(gl_log_format).string());
		} else {
			wxMessageBox(FromUTF8(boost::format(bloc::translate("Error: \"%1%\" cannot be found.")) %  game.Log(gl_log_format).string()),
			             translate("BOSS: Error"),
			             wxOK | wxICON_ERROR,
			             this);
		}
	} else {
		// Readme files.
		std::string file;
		// TODO(MCP): Look at converting this to a switch-statement
		if (event.GetId() == MENU_OpenMainReadMe)
			file = readme_path.string();
		else if (event.GetId() == MENU_OpenUserlistReadMe)
			file = rules_readme_path.string();
		else if (event.GetId() == MENU_OpenMasterlistReadMe)
			file = masterlist_doc_path.string();
		else if (event.GetId() == MENU_OpenVersionHistory)
			file = version_history_path.string();
		else if (event.GetId() == MENU_OpenLicenses)
			file = licenses_path.string();
		// Look for file.
		if (fs::exists(file)) {
			wxLaunchDefaultApplication(file);
		} else {  // No ReadMe exists, show a pop-up message saying so.
			wxMessageBox(FromUTF8(boost::format(bloc::translate("Error: \"%1%\" cannot be found.")) % file),
			             translate("BOSS: Error"),
			             wxOK | wxICON_ERROR,
			             this);
		}
	}
}

void MainFrame::OnAbout(wxCommandEvent &event) {
	wxAboutDialogInfo aboutInfo;
	aboutInfo.SetName("BOSS");
	aboutInfo.SetVersion(IntToString(BOSS_VERSION_MAJOR) + "." + IntToString(BOSS_VERSION_MINOR) + "." + IntToString(BOSS_VERSION_PATCH));
	aboutInfo.SetDescription(translate("Load order sorting for Oblivion, Skyrim, Fallout 3 and Fallout: New Vegas."));
	aboutInfo.SetCopyright("Copyright (C) 2009-2014 BOSS Development Team.");
	aboutInfo.SetWebSite("http://boss-developers.github.io");
	aboutInfo.SetLicence("This program is free software: you can redistribute it and/or modify\n"
	                     "it under the terms of the GNU General Public License as published by\n"
	                     "the Free Software Foundation, either version 3 of the License, or\n"
	                     "(at your option) any later version.\n"
	                     "\n"
	                     "This program is distributed in the hope that it will be useful,\n"
	                     "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
	                     "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
	                     "GNU General Public License for more details.\n"
	                     "\n"
	                     "You should have received a copy of the GNU General Public License\n"
	                     "along with this program.  If not, see <http://www.gnu.org/licenses/>.");
	//aboutInfo.SetIcon(wxIconLocation("BOSS.ico"));
	aboutInfo.SetIcon(wxICON(boss_icon));
	wxAboutBox(aboutInfo);
}

void MainFrame::OnRunTypeChange(wxCommandEvent &event) {
	// TODO(MCP): Look at converting this to a switch-statement
	if (event.GetId() == RADIOBUTTON_SortOption) {
		gl_revert = 0;
		gl_update_only = false;

		UpdateBox->Enable(true);
		TrialRunBox->Enable(true);

		RevertText->Enable(false);
		RevertChoice->Enable(false);
	} else if (event.GetId() == RADIOBUTTON_UpdateOption) {
		gl_revert = 0;
		gl_update_only = true;

		UpdateBox->Enable(false);
		TrialRunBox->Enable(false);
		RevertText->Enable(false);
		RevertChoice->Enable(false);
	} else {
		gl_revert = RevertChoice->GetSelection() + 1;
		gl_update_only = false;

		RevertText->Enable(true);
		RevertChoice->Enable(true);

		UpdateBox->Enable(false);
		TrialRunBox->Enable(false);
	}
	DisableUndetectedGames();  // Doesn't actually disable games if (gl_update_only).
}

void MainFrame::OnFormatChange(wxCommandEvent &event) {
	if (event.GetInt() == 0)
		gl_log_format = HTML;
	else
		gl_log_format = PLAINTEXT;
}

void MainFrame::OnGameChange(wxCommandEvent &event) {
	try {
		switch (event.GetId()) {
			case MENU_Oblivion:
				game = Game(OBLIVION);
				break;
			case MENU_Nehrim:
				game = Game(NEHRIM);
				break;
			case MENU_Skyrim:
				game = Game(SKYRIM);
				break;
			case MENU_Fallout3:
				game = Game(FALLOUT3);
				break;
			case MENU_FalloutNewVegas:
				game = Game(FALLOUTNV);
				break;
		}
	} catch (boss_error &e) {
		wxMessageBox(e.getString());
	}
	game.CreateBOSSGameFolder();
	SetTitle(wxT("BOSS - " + game.Name()));  // Don't need to convert name, known to be only ASCII chars.
}

void MainFrame::OnRevertChange(wxCommandEvent &event) {
	gl_revert = event.GetInt() + 1;
}

void MainFrame::OnLogDisplayChange(wxCommandEvent &event) {
	// MCP Note: Can the outside parentheses be removed?
	gl_silent = (!event.IsChecked());
}

void MainFrame::OnUpdateChange(wxCommandEvent &event) {
	gl_update = event.IsChecked();
}

void MainFrame::OnCRCDisplayChange(wxCommandEvent &event) {
	gl_show_CRCs = event.IsChecked();
}

void MainFrame::OnTrialRunChange(wxCommandEvent &event) {
	gl_trial_run = event.IsChecked();
}

void MainFrame::OnEditUserRules(wxCommandEvent &event) {
	if (gl_use_user_rules_manager) {
		UserRulesEditorFrame *editor = new UserRulesEditorFrame(translate("BOSS: User Rules Manager"),
		                                                        this,
		                                                        game);
		//editor->SetIcon(wxIconLocation("BOSS.ico"));
		editor->SetIcon(wxICON(boss_icon));
		editor->Show();
	} else {
		if (fs::exists(game.Userlist())) {
			wxLaunchDefaultApplication(game.Userlist().string());
		} else {
			try {
				RuleList userlist;
				userlist.Save(game.Userlist());
				wxLaunchDefaultApplication(game.Userlist().string());
			} catch (boss_error &e) {
				wxMessageBox(FromUTF8(boost::format(bloc::translate("Error: %1%")) % e.getString()),
				             translate("BOSS: Error"),
				             wxOK | wxICON_ERROR,
				             this);
			}
		}
	}
}

void MainFrame::OnClose(wxCloseEvent &event) {
	// Save settings to BOSS.ini before quitting.
	try {
		Settings ini;
		ini.Save(ini_path, game.Id());
	} catch (boss_error &e) {
			wxMessageBox(FromUTF8(boost::format(bloc::translate("Error: %1%")) % e.getString()),
			             translate("BOSS: Error"),
			             wxOK | wxICON_ERROR,
			             NULL);
	}

	Destroy();  // You may also do:  event.Skip();
	            // since the default event handler does call Destroy(), too
}

void MainFrame::SetGames(const Game &inGame,
                         const std::vector<std::uint32_t> inGames) {
	game = inGame;
	detectedGames = inGames;
	// TODO(MCP): Look at converting this to a switch-statement
	if (game.Id() == OBLIVION)
		GameMenu->FindItem(MENU_Oblivion)->Check();
	else if (game.Id() == NEHRIM)
		GameMenu->FindItem(MENU_Nehrim)->Check();
	else if (game.Id() == SKYRIM)
		GameMenu->FindItem(MENU_Skyrim)->Check();
	else if (game.Id() == FALLOUT3)
		GameMenu->FindItem(MENU_Fallout3)->Check();
	else if (game.Id() == FALLOUTNV)
		GameMenu->FindItem(MENU_FalloutNewVegas)->Check();

	std::size_t i = 0;
	for (i = 0; i < detectedGames.size(); i++) {
		if (detectedGames[i] == game.Id())
			break;
	}
	if (i == detectedGames.size()) {  // The current game wasn't in the list of detected games. Run in update only mode.
		gl_update_only = true;
		UpdateBox->Enable(false);
		TrialRunBox->Enable(false);
		SortOption->Enable(false);
		UpdateOption->SetValue(true);
		UndoOption->Enable(false);
	}
	DisableUndetectedGames();
}

void MainFrame::DisableUndetectedGames() {
	// Also disable the options for undetected games.
	bool enabled;
	if (gl_update_only)
		enabled = true;
	else
		enabled = false;
	GameMenu->FindItem(MENU_Oblivion)->Enable(enabled);
	GameMenu->FindItem(MENU_Nehrim)->Enable(enabled);
	GameMenu->FindItem(MENU_Skyrim)->Enable(enabled);
	GameMenu->FindItem(MENU_Fallout3)->Enable(enabled);
	GameMenu->FindItem(MENU_FalloutNewVegas)->Enable(enabled);
	for (std::size_t i = 0; i < detectedGames.size(); i++) {
		// TODO(MCP): Look at converting this to a switch-statement
		if (detectedGames[i] == OBLIVION)
			GameMenu->FindItem(MENU_Oblivion)->Enable();
		else if (detectedGames[i] == NEHRIM)
			GameMenu->FindItem(MENU_Nehrim)->Enable();
		else if (detectedGames[i] == SKYRIM)
			GameMenu->FindItem(MENU_Skyrim)->Enable();
		else if (detectedGames[i] == FALLOUT3)
			GameMenu->FindItem(MENU_Fallout3)->Enable();
		else if (detectedGames[i] == FALLOUTNV)
			GameMenu->FindItem(MENU_FalloutNewVegas)->Enable();
	}

	// Swapping from gl_update_only to !gl_update_only with undetected game active: need to change game to a detected game.
	if ((GameMenu->FindItem(MENU_Oblivion)->IsChecked() && !GameMenu->FindItem(MENU_Oblivion)->IsEnabled()) ||
	    (GameMenu->FindItem(MENU_Nehrim)->IsChecked() && !GameMenu->FindItem(MENU_Nehrim)->IsEnabled()) ||
	    (GameMenu->FindItem(MENU_Skyrim)->IsChecked() && !GameMenu->FindItem(MENU_Skyrim)->IsEnabled()) ||
	    (GameMenu->FindItem(MENU_Fallout3)->IsChecked() && !GameMenu->FindItem(MENU_Fallout3)->IsEnabled()) ||
	    (GameMenu->FindItem(MENU_FalloutNewVegas)->IsChecked() && !GameMenu->FindItem(MENU_FalloutNewVegas)->IsEnabled())) {
			if (!detectedGames.empty()) {
				// TODO(MCP): Look at converting this to a switch-statement
				if (detectedGames.front() == OBLIVION) {
					game = Game(OBLIVION);
					GameMenu->FindItem(MENU_Oblivion)->Check();
				} else if (detectedGames.front() == NEHRIM) {
					game = Game(NEHRIM);
					GameMenu->FindItem(MENU_Nehrim)->Check();
				} else if (detectedGames.front() == SKYRIM) {
					game = Game(SKYRIM);
					GameMenu->FindItem(MENU_Skyrim)->Check();
				} else if (detectedGames.front() == FALLOUT3) {
					game = Game(FALLOUT3);
					GameMenu->FindItem(MENU_Fallout3)->Check();
				} else if (detectedGames.front() == FALLOUTNV) {
					game = Game(FALLOUTNV);
					GameMenu->FindItem(MENU_FalloutNewVegas)->Check();
				}
				game.CreateBOSSGameFolder();
			}
	}
	SetTitle(wxT("BOSS - " + game.Name()));  // Don't need to convert name, known to be only ASCII chars.
}

}  // namespace boss
