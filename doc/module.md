# 功能模块 {#ZsWorld_Module}

# USD、几何与场景结构设计
- **credits** ：[ **王鑫磊** ](https://github.com/littlemine)
## 设计思路
## 具体说明

# 场景状态异步更新与可视化同步
- **credits** ：[ **王鑫磊** ](https://github.com/littlemine)
## 设计思路
## 具体说明

# 描边
- **credits** ：[ **邓瑞韬** ](https://github.com/scudrt)
## 算法思路
使用JFA(Jump Flood Algorithm)做描边，该算法的优点是通用，对mesh没有特殊要求，只要能渲染都可以做描边，而且开销良好。
## 具体流程
1. 使用简单的shader将待描边的物体渲染到一个单独的color attachment中，使用场景深度进行深度测试，关闭深度写入
2. 假设外扩像素值为n，做以下循环:
   1. n = n / 2
   2. 后处理，在当前color attachment中，采样当前像素以及横或纵距离为n的像素`(x∈[x - n, x, x + n], y∈[y - n, y, y + n])`，写入到另一个color attachment中
   3. 如果n==1，停止循环
   4. 将采样color attachment与当前color attachment互换
3. 将当前color attachment结果覆盖到主渲染color attachment中
## 其他
1. 参考：
   1. https://zznewclear13.github.io/posts/calculate-signed-distance-field-using-compute-shader/
   2. https://zhuanlan.zhihu.com/p/222901923
2. 具体实现在SceneEditorOutline.cpp中
3. 该算法需要进行log(N)次后处理
4. 外扩像素数量只有是2的次方时才是精确的n - 1，例如7会被算法分解为3 + 1，实际上只会外扩4个像素，而8则被分解为4 + 2 + 1，外扩7像素。

# 视锥剔除
- **credits** ：[ **邓瑞韬** ](https://github.com/scudrt)
## 算法思路
在CPU端测试待渲染物体的AABB是否与相机视锥相交，若存在相交则允许渲染
## 具体流程
1. 初始化mesh时遍历mesh顶点，计算物体的local space AABB
2. 渲染mesh前更新它的model matrix(skinning)
3. 将local space AABB通过model matrix转为world space AABB，因此这个world space AABB是不够紧凑的，但优点是计算比较快
4. 当前帧渲染前更新相机view矩阵，根据view矩阵以及fov等信息计算相机视锥体六个面的world space表示
5. 在渲染时获取物体的world space AABB，与相机视锥体的六个面分别进行测试，AABB在面内则表示物体在视锥内
## 其他
1. 参考：
   1. https://zhuanlan.zhihu.com/p/491340245
   2. https://zhuanlan.zhihu.com/p/97371838
2. 这里进一步优化了文章里面的思路，对于每一个视锥体面，仅取距离它最近的AABB顶点进行测试，而不是最远和最近点，计算量缩减一半

# Reversed-Z
- **开发者** ：[ **邓瑞韬** ](https://github.com/scudrt)
## 算法思路
float值距离0越近则精度越密集，越远则越稀疏。
在实际渲染中距离相机稍微有一定距离的片元，深度就已经超过0.9，场景中的大部分深度落在[0.9, 1.0]这个区间内，很容易因此出现z-fighting的现象。
通过倒转z深度值，将[0, 1]改为[1, 0]，可以使场景深度精度表示更均匀，大大减缓z-fighting问题。
## 具体流程
1. 如果开启reversedZ，则在setPerspective中交换far和near值
2. 使用glm::perspectiveZO，这个函数可以使深度映射到[0, 1]区间内
3. 在需要拿到深度的地方处理reversedZ的情形，例如各种深度测试。
## 其他
1. 参考
   1. https://tomhultonharrop.com/mathematics/graphics/2023/08/06/reverse-z.html
   2. https://zhuanlan.zhihu.com/p/341495234

# 遮挡剔除
- **credits** ：[**邓瑞韬**](https://github.com/scudrt)
## 算法思路
使用Vulkan的occlusion query做遮挡查询，对每一个mesh的AABB进行光栅化，根据query结果确定该物体是否完全被遮挡。
## 具体流程
1. 初始化occlusion query相关的上下文，具体实现见SceneEditorOcclusionQuery.cpp
2. 在渲染所有opaque物体之后进行遮挡查询，使用此时的深度图
3. 遮挡查询阶段提交物体的world space AABB做query，维护物体的occlusion id，将该物体的query结果记录在对应的occlusion id下
4. 对于被视锥剔除掉的物体，直接跳过它的遮挡查询
5. 在下一帧的opaque渲染前再查询结果，可以避免CPU等待GPU完成遮挡查询，但是由于未知的validation问题，现在的实现是在当帧拿到query result
## 其他
1. 参考
   1. 
2. 可以考虑使用场景树优化，把场景中物体的AABB进行合并，对于AABB小于某个阈值的树节点进行查询，可以有效避免大量小型物体产生过多的drawcall提交
3. 把被视锥剔除的物体标记为已通过遮挡查询，可以避免视野外的物体进入视野内的那一帧产生跳变
4. 可以考虑用compute shader去优化

# 透明渲染
- **credits** ：[ **邓瑞韬** ](https://github.com/scudrt)
## 算法思路
使用weighted OIT方法进行透明混合，该方法通过改变透明混合公式，解放了透明混合的顺序依赖，从而可以通过两次render pass实现OIT，而且不需要任何排序行为。
## 具体流程
## 其他
1. 参考
   1. https://zhuanlan.zhihu.com/p/487577680
   2. https://zhuanlan.zhihu.com/p/353940259
2. 该方法同时也导致透明物体有一定的色差(相对于真·半透明而言)，但是对于移动端而言是比较优的方法
3. 其他OIT方法有深度剥离、双向深度剥离、per-pixel linked lists等，是无损但是开销相对较高的方法

# 灯光管理
- **credits** ：[ **邓瑞韬** ](https://github.com/scudrt)
## 算法思路
为了实现高性能实时多光源，这里采用Clustered Forward Shading，也就是Forward+方式来做灯光渲染。
Forward+的核心就是它将视锥体划分为了多个cluster，每一个片元都有其所属的cluster，于是对于每一个片元，可以仅计算影响了所属cluster的光源，大大减少了光照计算量。
## 具体流程
1. 在CPU端，对灯光进行视锥剔除，将剔除后的灯光列表传给GPU，这一步是可选的
2. 在compute shader中根据灯光信息，计算灯光覆盖的cluster，当灯光覆盖了一个cluster，就将这个灯光加入这个cluster的灯光列表
3. 在mesh片元着色阶段，计算该片元对应的cluster id，获取这个cluster的灯光列表，依次遍历列表内的灯光信息进行着色计算
## 其他
1. 参考
2. ??

# 着色模型
- **credits** ：[ **邓瑞韬** ](https://github.com/scudrt)
## 算法思路
使用经典的Cook Torrance模型
## 具体流程
Color = (Kd * Lambert + D * F * G / (4.0 * NV * NL)) * Light
其中
Kd为漫反射系数
Lambert = Albedo / PI
D为法线分布函数，表示能够反射光线的微表面的比例
F为菲涅尔项，由视线与光线的角度关系衡量，反映经由微表面反射出的光线比例
G为几何函数，表示微表面中自遮挡的比例
Light = NL * intensity * lightColor
## 其他
1. 参考
   1. https://github.com/QianMo/PBR-White-Paper