local dir_path = path.relative(os.curdir(), os.projectdir())

-- package("check")
--
--     add_urls("git@github.com:libcheck/check.git")
--
--     on_install(function(package)
--         local configs = {}
--         table.insert(configs, "--enable-shared=" .. (package:config("shared") and "yes" or "no"))
--         table.insert(configs, "--enable-static=" .. (package:config("shared") and "no" or "yes"))
--         if package:debug() then
--             table.insert(configs, "--enable-debug")
--         end
--         table.insert(configs, "CFLAGS=-g -O0")
--         import("package.tools.autoconf").install(package, configs)
--     end)
-- package_end()
--
-- add_requires("check", { system = false, configs = { static = true }, debug = is_mode("debug") })

-- 遍历获取文件
-- 构建目标
target(dir_path)
    set_kind("binary")
    add_files("*.c")
    -- sudo apt-get install check
    add_links("check")
    -- add_packages("check")
