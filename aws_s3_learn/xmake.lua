local dir_path = path.relative(os.curdir(), os.projectdir())

-- 添加依赖包
add_requires("aws-c-common", {system = false, configs = {static = true}})
add_requires("aws-c-io", {system = false, configs = {static = true}})
add_requires("aws-c-http", {system = false, configs = {static = true}})
add_requires("aws-c-auth", {system = false, configs = {static = true}})
add_requires("aws-c-cal", {system = false, configs = {static = true}})
add_requires("aws-c-compression", {system = false, configs = {static = true}})
add_requires("aws-c-sdkutils", {system = false, configs = {static = true}})
add_requires("aws-checksums", {system = false, configs = {static = true}})
add_requires("aws-c-s3", {system = false, configs = {static = true}})
add_requires("s2n-tls", {system = false, configs = {static = true}})


-- 遍历获取文件
-- 构建目标
target(dir_path)
    set_kind("binary")
    add_files("*.c")
    add_packages(
        "aws-c-common",
        "aws-c-io", 
        "aws-c-http",
        "aws-c-auth",
        "aws-c-cal",
        "aws-c-compression",
        "aws-c-sdkutils",
        "aws-checksums",
        "aws-c-s3",
        "s2n-tls",
        {public = true}
    )
    add_ldflags("-static")
    set_languages("c99")
