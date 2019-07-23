#include "config.h"
#include "global.h"

int main(int argc, char* argv[])
try {

	// 解析命令行选项，读取配置文件
	parse_options(argc, argv);

    // 扫描源文件，搜集信息
    collect_info();
    if (collect_only)
        return 0;

	// todo: 额外的flags，是各个.cpp文件独立设置？还是？独立设置吧
    compile_cpp_cmd += extra_compile_flags;
    compile_h_cmd += extra_compile_flags;
    //gcc_link_cmd += extra_link_flags;

    // 构建
    bool success = build();
    if (build_only)
        return 0;

	// 运行
    if (success) {
        run();
    }

    return 0;
}
catch (const po::error& ex) {
	std::cerr << ex.what() << '\n';
	return 1;
}
catch (int exit_code) {
    return exit_code;
}
