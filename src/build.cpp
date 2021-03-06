#include "config.h"
#include "global.h"
#include "FileEntity.h"
#include "GccObj2ExeAction.h"
#include "VcObj2ExeAction.h"
#include "PhonyEntity.h"
#include "GccCpp2ObjAction.h"
#include "VcCpp2ObjAction.h"
#include "UpdateGraphAction.h"
#include "FollowRecipeAction.h"
#include "helpers.h"
#include "Loggers.h"
#include "H2GchAction.h"
#include "GchMagic.h"
#include "ShebangMagic.h"


bool is_one_of(fs::path file, const vector<fs::path>& file_set)
{
	if (std::find(std::begin(file_set), std::end(file_set), file) == std::end(file_set)) {
		return false;
	}

	return true;
}

void make_sure_these_at_the_head(vector<fs::path>& sources_to_pc, vector<fs::path>& sources)
{
	vector<fs::path> original_sources{ sources };
	sources.clear();
	sources = sources_to_pc;

	for (auto src : original_sources) {
		if (!is_one_of(src, sources_to_pc)) {
			sources.push_back(src);
		}

	}
}

void make_sure_this_at_the_head(fs::path& sources_to_pc, vector<fs::path>& sources)
{
	vector<fs::path> original_sources{ sources };
	sources.clear();
	sources.push_back(sources_to_pc);

	for (auto src : original_sources) {
		if (src != sources_to_pc) {
			sources.push_back(src);
		}

	}
}

bool build_exe()
{
	cmd_line_builder->add_libs(link_cmd_libs, libs);

	// 构建依赖关系图
	FileEntityPtr exe = makeFileEntity(exe_path);

	if (cc == CC::GCC || cc == CC::MINGW || cc == CC::CLANG) {
		exe->addAction(makeGccObj2ExeAction());
	}
	else if (cc == CC::VC) {
		exe->addAction(makeVcObj2ExeAction());
		//// 将sources中跟预编译头文件相关的.cpp提到到最前边，以便先行编译。
		//make_sure_these_at_the_head(sources_to_pc, sources);
		//assert(sources_to_pc.size() <= 1);
		if (vc_use_pch) {
			make_sure_this_at_the_head(vc_cpp_to_generate_pch, sources);
		}
		
	}
	else {
		assert(false);
	}

    // *************依赖关系图*************
	PhonyEntityPtr update_dependency = makePhonyEntity("update dependency graph");

	for (auto src_path : sources) {

		// 根据.cpp文件的名字，确定.o文件的名字
		fs::path obj_path = shadow(src_path);
		obj_path += cc_info[cc].obj_ext;

		FileEntityPtr obj = makeFileEntity(obj_path);
		if (cc == CC::GCC || cc == CC::MINGW || cc == CC::CLANG) {
			obj->addAction(makeGccCpp2ObjAction());
		}
		else if (cc == CC::VC) {
			obj->addAction(makeVcCpp2ObjAction());
		}
		else {
			assert(false);
		}

		// 可执行文件依赖.o文件
		exe->addPrerequisite(obj);

		// .o文件依赖.cpp文件
		FileEntityPtr src = makeFileEntity(src_path);
		obj->addPrerequisite(src);

		// 根据.o文件的名字，确定.d文件的名字
		fs::path dep_path = obj_path;
		dep_path += ".d";

		if (exists(dep_path)) {
			PhonyEntityPtr obj_update = makePhonyEntity("update for " + obj_path.string());
			obj_update->addAction(makeUpdateGraphAction(obj));
			update_dependency->addPrerequisite(obj_update);

			FileEntityPtr dep = makeFileEntity(dep_path);
			obj_update->addPrerequisite(dep);

		}


	} // for

	// 根据依赖关系图进行构建

	MINILOGBLK_IF(
		show_dep_graph, dep_graph_logger,
		os << endl;
	update_dependency->show(os);
	os << endl;
	);

	update_dependency->update();

    // *************用户自定义规则产生的文件*************
    // 诸如fluid这类工具生成的文件，把它们先产生出来
    // 理论上，这些文件是依赖关系图的一部分，只要update最终的exe就能自动生成
    // 但是，在cpps第一次执行某脚本时，并没有.d文件，所以.h文件并不在依赖图中。
    // 于是那些由fluild类工具生成的.h就无法纳入依赖图，从而挂在.h节点上用来生成.h的动作无法执行
    PhonyEntityPtr update_all_generated = makePhonyEntity("update all generated");
    for (auto r : user_defined_rules) {
        ActionPtr action = makeFollowRecipeAction(r); // 一条规则会产生多个文件(多个target)，这些文件关联的是共同的action
        for (auto t : r.targets) {
            //cout << "target:" << t << endl;
            FileEntityPtr a_target = makeFileEntity(t);
            for (auto p : r.prerequisites) {
                //cout << "prerequisite:" << p << endl;
                FileEntityPtr a_prerequisite = makeFileEntity(p);
                a_target->addPrerequisite(a_prerequisite);
            }
            //cout << "target: " << t << endl;
            a_target->addAction(action);
            update_all_generated->addPrerequisite(a_target);
        }
        //for (auto c : r.commands) {
        //    cout << "command:" << c << endl;
        //}
    }

    MINILOGBLK_IF(
        show_dep_graph, dep_graph_logger,
        os << endl;
    update_all_generated->show(os);
    os << endl;
    );

    update_all_generated->update();

    // *************exe文件*************
    MINILOGBLK_IF(
		show_dep_graph, dep_graph_logger,
		exe->show(os);
	os << endl;
	);

	bool success = exe->update();
	if (!success)
		return false;

	if (vm.count("output")) {
		fs::path output_path = output_name;
		if (!exists(output_path)) {
			copy(exe_path, output_path);
		}
		else if (!is_directory(output_path)) {
			remove(output_path);
			copy(exe_path, output_path);
		}
		else {
			cout << "a directory named " << output_name << " already exists" << endl;
		}

	}
	return true;
}

bool build_gch()
{
	if (headers_to_pc.empty())
		return true;

	PhonyEntityPtr all_gch = makePhonyEntity("generate all gch");
	PhonyEntityPtr update_dependency = makePhonyEntity("update dependency graph");

	for (auto header_path : headers_to_pc) {
		fs::path gch_path = header_path;
		gch_path += ".gch";



		FileEntityPtr gch = makeFileEntity(gch_path);
		gch->addAction(makeH2GchAction());

		//
		all_gch->addPrerequisite(gch);

		// .gch文件依赖.h文件
		FileEntityPtr header = makeFileEntity(header_path);
		gch->addPrerequisite(header);

		// 根据.gch文件的名字，确定.d文件的名字
		fs::path dep_path = shadow(gch_path);
		dep_path += ".d";

		if (exists(dep_path)) {
			PhonyEntityPtr gch_update = makePhonyEntity("update for " + gch_path.string());
			gch_update->addAction(makeUpdateGraphAction(gch));
			update_dependency->addPrerequisite(gch_update);

			FileEntityPtr dep = makeFileEntity(dep_path);
			gch_update->addPrerequisite(dep);
		}
	} // for

	// 根据依赖关系图进行构建
	MINILOGBLK_IF(
		show_dep_graph, dep_graph_logger,
		os << endl;
	update_dependency->show(os);
	os << endl;
	);

	update_dependency->update();

	MINILOGBLK_IF(
		show_dep_graph, dep_graph_logger,
		all_gch->show(os);
	os << endl;
	);

	bool success;
	success = all_gch->update();
	if (!success)
		return false;

	return true;
}

bool build()
{
	if (clear_run) {
		// 在build前删除所有的产生的文件
		safe_remove(exe_path);

		for (auto src : sources) {
			fs::path obj_path = shadow(src);
			obj_path += cc_info[cc].obj_ext;
			safe_remove(obj_path);

			fs::path dep_path = obj_path;
			dep_path += ".d";
			safe_remove(dep_path);

			fs::path bc_path = obj_path;
			bc_path += ".birthcert";
			safe_remove(bc_path);
		}

		for (auto h : headers_to_pc) {
			fs::path gch_path = shadow(h);
			gch_path += cc_info[cc].pch_ext;
			safe_remove(gch_path);

			fs::path dep_path = gch_path;
			dep_path += ".d";
			safe_remove(dep_path);

			fs::path bc_path = gch_path;
			bc_path += ".birthcert";
			safe_remove(bc_path);
		}
	}

	bool success;

	if (cc == CC::GCC || cc == CC::MINGW || cc == CC::CLANG) {
		GchMagic gch_magic(headers_to_pc);
		success = build_gch();
		if (!success)
			return false;
	}


	ShebangMagic shebang_magic(script_file.string());
	success = build_exe();
	if (!success)
		return false;

	return true;
}
