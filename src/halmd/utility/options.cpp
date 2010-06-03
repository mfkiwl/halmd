/*
 * Copyright © 2008-2010  Peter Colberg
 *
 * This file is part of HALMD.
 *
 * HALMD is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <fstream>
#include <iostream>

#include <halmd/utility/module/factory.hpp>
#include <halmd/utility/options.hpp>
#include <halmd/version.h>

using namespace boost;
using namespace boost::assign;
using namespace std;

namespace halmd
{
namespace po
{

using boost::program_options::collect_unrecognized;
using boost::program_options::command_line_parser;
using boost::program_options::error;
using boost::program_options::include_positional;
using boost::program_options::notify;
using boost::program_options::parse_config_file;
using boost::program_options::parsed_options;
using boost::program_options::store;

/**
 * parse global program option values
 */
void parse_options(int argc, char** argv, options& vm)
{
    po::options_description desc("Program options");
    desc.add_options()
        ("output,o",
         po::value<string>()->default_value(to_lower_copy(string(PROGRAM_NAME)) + "_%Y%m%d_%H%M%S"),
         "output file prefix")
        ("input,I", po::value<vector<string> >(),
         "parameter input file")
        ("trajectory,J", po::value<string>(),
         "trajectory input file")
        ("verbose,v", po::accum_value<int>()->default_value(0),
         "increase verbosity")
        ("version",
         "output version and exit")
#if 0 // FIXME
        ("help",
         "display this help and exit")
#endif
        ;

    po::unparsed_options unparsed;

    try {
        po::command_line_parser parser(argc, argv);
        po::parsed_options parsed(parser.options(desc).allow_unregistered().run());
        po::store(parsed, vm);
        // FIXME only copy option if unregistered
        std::copy(
            parsed.options.begin()
          , parsed.options.end()
          , std::back_inserter(unparsed.command_line_options)
        );

        // parse optional parameter input files
        if (vm.count("input")) {
            BOOST_FOREACH(string const& fn, vm["input"].as<vector<string> >()) {
                ifstream ifs(fn.c_str());
                if (ifs.fail()) {
                    cerr << PROGRAM_NAME ": could not open parameter input file '" << fn << "'\n";
                    throw options_parser_error(EXIT_FAILURE);
                }

                po::parsed_options parsed(po::parse_config_file(ifs, desc, true));
                po::store(parsed, vm);
                unparsed.config_file_options.push_back(std::vector<po::option>());
                // FIXME only copy option if unregistered
                std::copy(
                    parsed.options.begin()
                  , parsed.options.end()
                  , std::back_inserter(unparsed.config_file_options.back())
                );
            }
        }
    }
    catch (po::error const& e) {
        cerr << PROGRAM_NAME ": " << e.what() << "\n";
        cerr << "Try `" PROGRAM_NAME " --help' for more information.\n";
        throw options_parser_error(EXIT_FAILURE);
    }

    if (vm.count("help")) {
        cout << "Usage: " PROGRAM_NAME " [OPTION]...\n" << desc << "\n";
    }

    if (vm.count("version")) {
        cout << PROGRAM_NAME " (" PROGRAM_DESC ") " PROGRAM_VERSION "\n"
            "\n" PROGRAM_COPYRIGHT "\n" "This is free software. "
            "You may redistribute copies of it under the terms of\n"
            "the GNU General Public License "
            "<http://www.gnu.org/licenses/gpl.html>.\n"
            "There is NO WARRANTY, to the extent permitted by law.\n";
        throw options_parser_error(EXIT_SUCCESS);
    }

    // FIXME dirty hack
    utility::module::factory::stack_.push_back(unparsed);
}

/**
 * parse module program option values
 */
void parse_options(unparsed_options& unparsed, options_description const& opt, options& vm)
{
    try {
        po::command_line_parser parser(po::collect_unrecognized(unparsed.command_line_options, po::include_positional));
        po::store(parser.options(opt).allow_unregistered().run(), vm);

        // parse unrecognised options from parameter input files
        BOOST_FOREACH(std::vector<po::option> const& unparsed_, unparsed.config_file_options) {
            po::parsed_options parsed(&opt);
            std::copy(unparsed_.begin(), unparsed_.end(), std::back_inserter(parsed.options));
            po::store(parsed, vm);
        }
    }
    catch (po::error const& e) {
        cerr << PROGRAM_NAME ": " << e.what() << "\n";
        cerr << "Try `" PROGRAM_NAME " --help' for more information.\n";
        throw options_parser_error(EXIT_FAILURE);
    }

    po::notify(vm);
}

} // namespace po

} // namespace halmd
