//
// Copyright 2010-2011 Ettus Research LLC
// Copyright 2018 Ettus Research, a National Instruments Company
//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#include "user_regs.hpp"

#include <boost/program_options.hpp>
#include <chrono>
#include <iostream>
#include <thread>
#include <uhd/usrp/multi_usrp.hpp>
#include <uhd/utils/safe_main.hpp>

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

int UHD_SAFE_MAIN(int argc, char * argv[]) {
	std::cout << "Creating the USRP device..." << std::endl;
	uhd::usrp::multi_usrp::sptr usrp;
	std::string usrp_args = "";
	usrp                  = uhd::usrp::multi_usrp::make(usrp_args);

	std::cout << "Using Device: " << usrp->get_pp_string() << std::endl;

	std::string call = argv[1];

	std::string fname;
	std::vector<std::string> args;
	std::unordered_map<std::string, std::function<void()>> dispatch;

	dispatch["set_sr_core_play"] = [&]() {
		if (args.size() != 2) throw std::runtime_error("set_sr_core_play(enable, trigger_src)");
		set_sr_core_play(usrp, to_u32(args[0]), to_u32(args[1]));
	};

	if (!parse_call(call, fname, args)) {
		std::cerr << "Bad syntax. Expected function_name(arg1, arg2)\n";
		return 1;
	}

	try {
		auto it = dispatch.find(fname);
		if (it == dispatch.end()) {
			std::cerr << "Unknown function: " << fname << "\n";
			return 1;
		}

		it->second();
	} catch (const std::exception & e) {
		std::cerr << "Error: " << e.what() << "\n";
		return 1;
	}

	return EXIT_SUCCESS;
}
