# 构建 {#ZsWorld_Build}

 
## 配置依赖

拉取项目代码：
```console
git clone https://github.com/zenustech/zs-world.git --recursive
```

绝大多数依赖通过子模块引入一并构建。若clone时未带--recursive选项，则需手动更新子模块：
```console
git submodule update --init --recursive
```

此外还有少量第三方依赖，如[vulkan](https://vulkan.lunarg.com/), boost filesystem/process等，是以[find_package](https://cmake.org/cmake/help/latest/command/find_package.html)形式引用。
推荐开发者通过包管理器（apt、vcpkg、brew/macports等）安装或手动从源码构建使用。

### USD支持
#### USD二进制库获取
##### 通过源码编译(推荐)
首先通过github克隆usd源码:
```
git clone https://github.com/PixarAnimationStudios/OpenUSD.git
``` 
假定这些源码存放在`USD_Source`路径下, 建议使用usd官方提供的python脚本进行编译, 需要安装`curl`用以下载编译过程依赖的资源, 脚本位于`${USD_Source}/build_scripts/build_usd.py`
这里提供一个推荐的最小所需功能集编译命令:
```shell
python ${USD_Source}/build_scripts/build_usd.py \
--no-python --no-tests --no-examples --no-tutorials \
--no-tools --no-docs --imaging --build-monolithic \
--src ${Depend_Source} ${USD_Install}
```
其中,`USD_Install`即为编译出的二进制结果及其头文件储存的文件夹,  `Depend_Source`用来保存usd编译所需的外部库源代码(如`tbb`), 不要被前面的`--src`迷惑, 此处**不能**设为与`USD_Source`相同路径, 建议设置为不在`USD_Install`下的外部自定义路径, 有助于维持`USD_Install`目录结构可读性并方便下一步配置.
较为推荐的路径结构可以参考:
```

...
|
|-- USD_Source
|
|-- Depend_Source
|
|-- USD_Install
|
|-- 本项目源代码目录
```
更多编译选项可以参照
```shell
python ${USD_Source}/build_scripts/build_usd.py --help
```
以及OpenUSD[官方编译文档](https://github.com/PixarAnimationStudios/OpenUSD/blob/release/BUILDING.md).
##### 下载预编译二进制
TODO
#### USD环境配置

USD依赖可通过以下数种方式来配置：
- 将前述`USD_Install`路径加入系统PATH(不推荐,可能干扰blender houdini等软件运行)
- 将`USD_Install`里的`bin`(不存在则忽略),`lib`,`include`等文件夹手动拷贝至“项目根目录/externals/USD”目录下
- 项目构建时添加cmake选项`-DZS_USD_ROOT=${USD_Install}`(即前述USD库安装目录)

## 构建项目

```console
cmake -Bbuild -DZS_ENABLE_USD=ON -DZS_USD_ROOT=${USD_Install}
cmake --build build --config Release --parallel 12 --target zs_world
```

- **注意**：若USD模块未配置好，可设置-DZS_ENABLE_USD=OFF来关闭相关模块。
- **注意**：若想启用precompile header来加速编译，可在cmake configure时加入-DZS_ENABLE_PCH=ON选项。

## 文档构建
项目文档可见README.md以及doc目录，而服务于开发的API文档需自行安装doxygen并通过cmake构建。

```console
cmake -Bbuild -DZS_WORLD_ENABLE_DOC=ON
cmake --build build --target zs_world_doc
```
