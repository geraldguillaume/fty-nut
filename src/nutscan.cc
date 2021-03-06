/*  =========================================================================
    nutscan - Wrapper around nut-scanner tool

    Copyright (C) 2014 - 2017 Eaton

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
    =========================================================================
*/

/*
@header
    nutscan - Wrapper around nut-scanner tool
@discuss
@end
*/

#include <fty_common_mlm_subprocess.h>
#include "nutscan.h"
#include <fty_log.h>

#include <sstream>
#include <string>
#include <vector>

/**
 * \brief one read line from istream
 */
static
ssize_t
s_readline(
        std::istream& inp,
        std::string& line)
{
    std::ostringstream buf;
    buf.clear();
    std::size_t ret;
    if (!inp.good() || inp.eof()) {
        return -1;
    }

    for (ret = 0; inp.good() && inp.peek() != '\n'; ret++) {
        char ch = static_cast<char>(inp.get());
        if (inp.eof())
            break;
        buf << ch;
    }
    if (inp.peek() == '\n') {
        buf << static_cast<char>(inp.get());
    }

    line.append(buf.str());
    return ret;
}

/**
 * \brief parse an output of nut-scanner
 *
 * \param name - name of device in the result
 * \param inp  - input stream
 * \param out  - vector of string with output
 */
static
void
s_parse_nut_scanner_output(
        const std::string& name,
        std::istream& inp,
        std::vector<std::string>& out)
{

    if (!inp.good() || inp.eof())
        return;

    std::stringstream buf;
    bool got_name = false;

    while (inp.good() && !inp.eof()) {
        std::string line;

        if (s_readline(inp, line) == -1)
            break;

        if (line.size() == 0)
            continue;

        if (line[0] == '\n')
            continue;

        if (line[0] == '[') {
            // New snippet begins, flust old data to out (if any)
            if (buf.tellp() > 0) {
                out.push_back(buf.str());
                buf.clear();
                buf.str("");
            }
            // Do not flush the name into buf here just yet -
            // do so if we have nontrivial config later on
            got_name = true;
        }
        else {
            if (got_name) {
                buf << '[' << name << ']' << std::endl;
                got_name = false;
            }
            if (buf.tellp() > 0)
                buf << line;
        }
    }

    if (got_name && buf.tellp() == 0)
        log_error ("While parsing nut-scanner output for %s, got a section tag but no other data", name.c_str());

    if (buf.tellp() > 0) {
        out.push_back(buf.str());
        buf.clear();
        buf.str("");
    }
}

/**
 * \brief run nut-scanner binary and return the output
 */
static
int
s_run_nut_scanner(
    const MlmSubprocess::Argv& args,
    const std::string& name,
    std::vector<std::string>& out)
{
    std::string o;
    std::string e;
    log_debug ("START: nut-scanner with timeout 10 ...");
    int ret = MlmSubprocess::output(args, o, e, 10);
    log_debug ("       done with code %d and following stdout:\n-----\n%s\n-----\n       ...and stderr:\n-----\n%s\n-----\n", ret, o.c_str(), e.c_str());
    if (ret != 0) {
        log_error("Execution of nut-scanner FAILED with code %d and %s",
            ret, e.empty() ? "no message" : ("message" + e).c_str());
    }
    if (ret == 0 && !e.empty()) {
        log_debug("Execution of nut-scanner SUCCEEDED with message %s",
            e.c_str());
    }

    if (ret != 0)
        return -1;

    std::istringstream inp{o};
    s_parse_nut_scanner_output(
            name,
            inp,
            out);

    if (out.empty()) {
        log_info("No suggestions from nut-scanner for device %s", name.c_str());
        return -1;
    }

    return 0;
}

int
nut_scan_snmp(
        const std::string& name,
        const CIDRAddress& ip_address,
        const std::string community,
        bool use_dmf,
        std::vector<std::string>& out)
{
    std::string comm;
    comm = community;
    if (comm.empty())
        comm = "public";

    int r = -1;
    // DMF enabled and available
    if (use_dmf || ::getenv ("BIOS_NUT_USE_DMF")) {
        MlmSubprocess::Argv args = {"nut-scanner", "-q", "--community", comm, "-z", "-s", ip_address.toString()};
        log_debug("nut-scanning SNMP device at %s using DMF support", ip_address.toString().c_str());
        r = s_run_nut_scanner(
                args,
                name,
                out);

        if (r != -1)
            return r;
    }

    // DMF not available
    MlmSubprocess::Argv args = {"nut-scanner", "-q", "--community", comm, "-S", "-s", ip_address.toString()};
    log_debug("nut-scanning SNMP device at %s using legacy mode", ip_address.toString().c_str());
    r = s_run_nut_scanner(
            args,
            name,
            out);
    return r;
}


int
nut_scan_xml_http(
        const std::string& name,
        const CIDRAddress& ip_address,
        std::vector<std::string>& out)
{
    MlmSubprocess::Argv args = {"nut-scanner", "-q", "-M", "-s", ip_address.toString()};
    log_debug("nut-scanning NetXML device at %s", ip_address.toString().c_str());
    return s_run_nut_scanner(
            args,
            name,
            out);
}

//  --------------------------------------------------------------------------
//  Self test of this class

void
nutscan_test (bool verbose)
{
    printf (" * nutscan: ");

    //  @selftest
    //  Simple create/destroy test
    //  @end
    printf ("OK\n");
}
