//
// Copyright 2010-2011 Ettus Research LLC
// Copyright 2018 Ettus Research, a National Instruments Company
//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#include "user_regs.hpp"

#include <boost/program_options.hpp>
#include <cctype>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <readline/history.h>
#include <readline/readline.h>
#include <sstream>
#include <string>
#include <thread>
#include <uhd/usrp/multi_usrp.hpp>
#include <uhd/utils/safe_main.hpp>
#include <unordered_map>
#include <vector>

static volatile bool running = true;
static std::vector<std::string> command_list;
static std::unordered_map<std::string, std::function<void(const std::vector<std::string> &)>> dispatch;
uhd::usrp::multi_usrp::sptr usrp;

void signal_handler(int sig) {
	if (sig == SIGINT) {
		std::cout << "\nExiting...\n";
		running = false;
		rl_done = 1; // Tell readline to stop
	}
}

static std::string trim(const std::string & s) {
	size_t b = 0, e = s.size();
	while (b < e && std::isspace((unsigned char)s[b]))
		++b;
	while (e > b && std::isspace((unsigned char)s[e - 1]))
		--e;
	return s.substr(b, e - b);
}

static std::vector<std::string> split_args(const std::string & s) {
	std::vector<std::string> out;
	std::string cur;
	int depth = 0;

	for (char c: s) {
		if (c == ',' && depth == 0) {
			out.push_back(trim(cur));
			cur.clear();
		} else {
			if (c == '(')
				++depth;
			else if (c == ')')
				--depth;
			cur += c;
		}
	}
	if (!cur.empty()) out.push_back(trim(cur));
	return out;
}

static bool parse_call(const std::string & input, std::string & name, std::vector<std::string> & args) {
	auto lp = input.find('(');
	auto rp = input.rfind(')');
	if (lp == std::string::npos || rp == std::string::npos || rp < lp) return false;

	name               = trim(input.substr(0, lp));
	std::string inside = input.substr(lp + 1, rp - lp - 1);
	inside             = trim(inside);

	if (inside.empty()) {
		args.clear();
		return true;
	}

	args = split_args(inside);
	return true;
}

static uint32_t to_u32(const std::string & s) {
	return static_cast<uint32_t>(std::stoul(s));
}

static float to_f32(const std::string & s) {
	return std::stof(s);
}

// Autocompletion function for readline
// Fix the warning in command_generator (line 96)
char * command_generator(const char * text, int state) {
	static int idx, len;
	static std::vector<std::string> matches;

	if (state == 0) {
		matches.clear();
		len = strlen(text);
		for (const auto & cmd: command_list) {
			if (strncmp(cmd.c_str(), text, len) == 0) {
				matches.push_back(cmd);
			}
		}
		idx = 0;
	}

	if (idx < (int)matches.size()) { // Cast to int to fix warning
		return strdup(matches[idx++].c_str());
	}

	return nullptr;
}

char ** reg_completion(const char * text, int start, int end) {
	char ** matches = nullptr;
	(void)end; // Suppress unused parameter warning

	// Only complete at the beginning of the line (not in arguments)
	if (start == 0) {
		matches = rl_completion_matches(text, command_generator);

		// Add "(" after completion if there's exactly one match
		if (matches && matches[0] && !matches[1]) {
			char * completed  = matches[0];
			size_t len        = strlen(completed);
			char * with_paren = (char *)malloc(len + 2);
			strcpy(with_paren, completed);
			with_paren[len]     = '(';
			with_paren[len + 1] = '\0';
			free(matches[0]);
			matches[0] = with_paren;
		}
	}

	return matches;
}

// Initialize all commands
void init_commands() {
	// Get commands (all with empty lambdas for now - will be replaced)
	command_list                 = {"set_sr_core_play", "help", "exit", "quit", "clear"};

	dispatch["set_sr_core_play"] = [](const std::vector<std::string> & args) {
		if (args.size() != 2) throw std::runtime_error("set_sr_core_play(enable, trigger_src)");
		set_sr_core_play(usrp, to_u32(args[0]), to_u32(args[1]));
		std::cout << "OK\n";
	};

	// Utility commands
	dispatch["help"] = [](const std::vector<std::string> &) {
		std::cout << "Available commands:\n";
		for (const auto & cmd: command_list) {
			if (cmd != "help" && cmd != "exit" && cmd != "quit" && cmd != "clear") {
				std::cout << "  " << cmd << "()\n";
			}
		}
		std::cout << "\nUtility commands:\n";
		std::cout << "  help - show this help\n";
		std::cout << "  clear - clear screen\n";
		std::cout << "  exit, quit - exit program\n";
		std::cout << "\nTips:\n";
		std::cout << "  - Press TAB to autocomplete commands\n";
		std::cout << "  - Use Up/Down arrows for command history\n";
		std::cout << "  - Press Ctrl+C to exit\n";
	};

	dispatch["exit"]  = [](const std::vector<std::string> &) { running = false; };

	dispatch["quit"]  = [](const std::vector<std::string> &) { running = false; };

	dispatch["clear"] = [](const std::vector<std::string> &) {
		std::cout << "\033[2J\033[1;1H"; // Clear screen
	};
}


int UHD_SAFE_MAIN(int argc, char * argv[]) {
	// Setup signal handler
	std::signal(SIGINT, signal_handler);

	// Initialize readline
	rl_readline_name                 = "reg";
	rl_attempted_completion_function = reg_completion;

	// Initialize commands
	init_commands();
	std::cout << "Creating the USRP device..." << std::endl;
	std::string usrp_args = "type = b200, enable_user_regs ";
	usrp                  = uhd::usrp::multi_usrp::make(usrp_args);

	std::cout << "Using Device: " << usrp->get_pp_string() << std::endl;

	std::cout << "\n\x1b[36mU220 registers CLI Started\x1b[0m\n";
	std::cout << "Type 'help' for commands, TAB for autocomplete, Ctrl+C to exit\n\n";

	// Load command history
	using_history();
	read_history(".reg_history");


	while (running) {
		char * input_cstr = readline("\x1b[32mreg>\x1b[0m ");

		if (!input_cstr) {
			// Ctrl+D pressed
			std::cout << "\n";
			break;
		}

		std::string input = trim(input_cstr);
		free(input_cstr);

		if (input.empty()) {
			continue;
		}

		// Add to history
		add_history(input.c_str());

		// Parse the command
		std::string fname;
		std::vector<std::string> args;

		// Auto-add parentheses if missing and not a utility command
		if (input.find('(') == std::string::npos && input != "help" && input != "exit" && input != "quit" && input != "clear") {
			input = input + "()";
		}

		if (!parse_call(input, fname, args)) {
			// Check if it's a utility command without parentheses
			if (dispatch.find(input) != dispatch.end()) {
				fname = input;
				args.clear();
			} else {
				std::cerr << "Bad syntax. Expected function_name(arg1, arg2)\n";
				continue;
			}
		}

		// Execute the command
		auto it = dispatch.find(fname);
		if (it != dispatch.end()) {
			try {
				it->second(args);
			} catch (const std::exception & e) {
				std::cerr << "Error: " << e.what() << "\n";
			}
		} else {
			std::cerr << "Unknown command: " << fname << "\n";
		}
	}

	// Save history
	write_history(".reg_history");

	std::cout << "\nGoodbye!\n";

	return EXIT_SUCCESS;
}
