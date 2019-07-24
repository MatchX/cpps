#include "config.h"
#include "global.h"
#include "Loggers.h"
#include "helpers.h"
#include "InfoPackageScanned.h"
#include "sha1.hpp"

#ifdef HAVE_REGEX
using std::regex;
using std::smatch;
#else
using boost::regex;
using boost::smatch;
#endif

void scan(fs::path src_path, InfoPackageScanned& pack)
{
    pack.cpp_sig = SHA1::from_file(src_path.string());

	MINILOG(build_exe_summay_logger, "scanning " << src_path.filename().string());

	ifstream in(src_path.string());

	string line;

	regex usingcpp_pat{ R"(^\s*#include\s+"([\w\./]+)\.h"\s+//\s+usingcpp)" };
	regex using_pat{ R"(using\s+([\w\./]+\.(cpp|cxx|c\+\+|cc|c)))" }; // | 或的顺序还挺重要，把长的排前边。免得前缀就匹配。
	regex linklib_pat{ R"(//\s+linklib\s+([\w\-\.]+))" };

	string compiler_specific_linklib_string = R"(//\s+)" + cc_info[cc].compiler_name;
	compiler_specific_linklib_string += R"(-linklib\s+([\w\-\.]+))";
	regex compiler_specific_linklib_pat{ compiler_specific_linklib_string };

	// 下面这行，前后三个*号，不是正则的一部分，而是c++ raw-string的自定义delimiter
	// https://stackoverflow.com/questions/49416631/escape-r-in-a-raw-string-in-c
	regex precompile_pat{ R"***(^\s*#include\s+"([\w\./]+\.(h|hpp|H|hh))"\s+//\s+precompile)***" };

	string compiler_specific_extra_compile_flags_string = R"(//\s+)" + cc_info[cc].compiler_name;
	compiler_specific_extra_compile_flags_string += R"(-extra-compile-flags:\s+(.*)$)";
	regex compiler_specific_extra_compile_flags_pat{ compiler_specific_extra_compile_flags_string };

	string compiler_specific_extra_link_flags_string = R"(//\s+)" + cc_info[cc].compiler_name;
	compiler_specific_extra_link_flags_string += R"(-extra-link-flags:\s+(.*)$)";
	regex compiler_specific_extra_link_flags_pat{ compiler_specific_extra_link_flags_string };


	int n = 0;
	while (getline(in, line)) {
		smatch matches;
		// 搜集引用的.cpp文件
		if (regex_search(line, matches, usingcpp_pat)) {
			string cpp_file_name = matches[1];
			cpp_file_name += ".cpp";
			MINILOG(collect_info_detail_logger, "found: " << cpp_file_name);
			// 一个.cpp文件中引用的另一个.cpp文件，是以相对路径的形式指明的
			fs::path a = src_path;
			a.remove_filename();
			a /= cpp_file_name;
            pack.referenced_sources.push_back(a);
		}
		else if (regex_search(line, matches, using_pat)) {
			MINILOG(collect_info_detail_logger, "found: " << matches[1]);
			// 一个.cpp文件中引用的另一个.cpp文件，是以相对路径的形式指明的
			fs::path a = src_path;
			a.remove_filename();
			a /= matches.str(1);
            pack.referenced_sources.push_back(a);
		}

		// 搜集使用的库名字
		if (regex_search(line, matches, linklib_pat)) {
			MINILOG(collect_info_detail_logger, "found lib: " << matches[1]);
			pack.referenced_libs.push_back(matches[1]);
		}

		if (regex_search(line, matches, compiler_specific_linklib_pat)) {
			MINILOG(collect_info_detail_logger, "found lib: " << matches[1]);
			pack.referenced_libs.push_back(matches[1]);
		}

		if (regex_search(line, matches, compiler_specific_extra_compile_flags_pat)) {
            MINILOG(collect_info_detail_logger, "found extra link flags: " << matches[1]);
            string flags = " ";
            flags += matches[1];
            pack.referenced_compiler_specific_extra_compile_flags[src_path.string()] += flags;
		}

		if (regex_search(line, matches, compiler_specific_extra_link_flags_pat)) {
            MINILOG(collect_info_detail_logger, "found extra compile flags: " << matches[1]);
            string flags = " ";
            flags += matches[1];
            pack.referenced_compiler_specific_extra_link_flags += flags;
        }

		// 搜集需要预编译的头文件
		if (regex_search(line, matches, precompile_pat)) {
			MINILOG(collect_info_detail_logger, "found pch: " << matches[1]);
			// 一个.cpp文件要求预编译某个头文件，是以相对路径的形式指明的
			fs::path a = src_path;
			a.remove_filename();
			a /= matches.str(1);
            pack.referenced_headers_to_pc.push_back(a);
            if (cc == CC::VC) {
                pack.referenced_vc_use_pch = true;
                pack.referenced_vc_h_to_precompile = a;
                pack.referenced_vc_cpp_to_generate_pch = src_path;
            }
		}


		if (max_line_scan != -1) {
			n++;
			if (n >= max_line_scan)
				break;
		}

	}

}

void check_referenced_file(fs::path src_path, InfoPackageScanned& pack)
{
    for (auto p : pack.referenced_sources) {
        if (!exists(p)) {
            cout << p << " referenced by " << src_path << " does NOT exsit!" << endl;
            throw 1;
        }
    }

    for (auto p : pack.referenced_headers_to_pc) {
        if (!exists(p)) {
            cout << p << " referenced by " << src_path << " does NOT exsit!" << endl;
            throw 1;
        }
    }

}

void merge(const InfoPackageScanned& pack)
{
    copy(pack.referenced_libs.begin(), pack.referenced_libs.end(), std::back_inserter(libs));
    compiler_specific_extra_compile_flags.insert(pack.referenced_compiler_specific_extra_compile_flags.begin(), pack.referenced_compiler_specific_extra_compile_flags.end());
    compiler_specific_extra_link_flags += pack.referenced_compiler_specific_extra_link_flags;
    //copy(pack.referenced_headers_to_pc.begin(), pack.referenced_headers_to_pc.end(), std::back_inserter(headers_to_pc));
    for (auto p : pack.referenced_headers_to_pc) {
        if (find(headers_to_pc.begin(), headers_to_pc.end(), p) == headers_to_pc.end()) {
            headers_to_pc.push_back(p);
        }
    }
    
    vc_use_pch = pack.referenced_vc_use_pch;
    vc_h_to_precompile = pack.referenced_vc_h_to_precompile;
    vc_cpp_to_generate_pch = pack.referenced_vc_cpp_to_generate_pch;
}

void collect_source(fs::path src_path)
{
    // 如果已经采集过该文件，就不要再采集，避免循环引用
    if (find(sources.begin(), sources.end(), src_path) != sources.end()) {
        MINILOG(collect_info_summary_logger, src_path << " is already collected.");
        return;
    }

    boost::timer::cpu_timer timer;

    MINILOG(collect_info_summary_logger, "collecting " << src_path.filename().string());

    sources.push_back(src_path);


    InfoPackageScanned pack;
    // load or scan
    fs::path pack_path = shadow(src_path);
    pack_path += ".scan";

    if (clear_run) {
        safe_remove(pack_path);
    }

    bool needScan = true;

    if (exists(pack_path)) {
        //cout << pack_path << " pack exists" << endl;
        ifstream ifs(pack_path.string());
        assert(ifs);

        boost::archive::text_iarchive ia(ifs);

        ia >> pack;

        // 如果.cpp在上一次扫描生成信息包后并未发生改变，那就不用重新扫描了
        string cpp_sig = SHA1::from_file(src_path.string());
        if (pack.cpp_sig == cpp_sig) {
            needScan = false;
        }
    }

    if (needScan) {
        //cout << pack_path << " pack NOT exists" << endl;
        scan(src_path, pack);

        // 确保目录存在
        create_directories(pack_path.parent_path());

        //cout << pack_path << " 2pack NOT exists" << endl;
        ofstream ofs(pack_path.string());
        assert(ofs);
        boost::archive::text_oarchive oa(ofs);

        oa << pack;

    }
    //else {
    //    cout << "loading pack of " << src_path.filename().string() << endl;
    //}

    // 检查.cpp中引用的文件是否存在
    check_referenced_file(src_path, pack);

    // 将采集到的部分信息合并到整体中
    merge(pack);

    MINILOG(build_exe_timer_logger, "collecting " << src_path.filename() 
        << " "
        << timer.format(boost::timer::default_places, "%ws"));

    // 采集引用的.cpp文件
    for (auto a : pack.referenced_sources) {
        collect_source(a);
    }

}

void collect()
{
	// 确定可执行文件的路径
	fs::path script_path = canonical(script_file).make_preferred();
	exe_path = shadow(script_path);
	exe_path += "." + cc_info[cc].compiler_name;
	exe_path += ".exe";

	// 确定所有.cpp文件的路径；确定所有库的名字；确定所有的预编译头文件的路径
	collect_source(canonical(script_path).make_preferred());
	//    cout << "***" << script_file << endl;
	//    cout << "***" << script_path.make_preferred() << endl;
	//    cout << "***" << canonical(script_path) << endl;

}
