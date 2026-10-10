#version 330 core
// 上面是指定OpenGL 3.3 核心模式，必须写在第一行
// in：顶点属性，从VAO/VBO读取，每个顶点一个值
// layout (location = x) 指的这个属性对应的GLSL哪个槽位，
// 和 createVAO 里面 glVertexAttribPointer 的第一个参数 location 对应
// vec3 是三个float的向量。
layout (location = 0) in vec3 aPos; // 顶点的局部坐标
layout (location = 1) in vec3 aNormal; // 顶点的局部法线
// out 表示这个变量会传给片元着色器
// 顶点着色器的 out 和片元着色器的 in 名字必须一致
// out 传到片元着色器后，GPU 会对顶点间的值做插值（每个片元得到顶点间平滑过渡的值）
out vec3 FragPos; // 顶点的世界坐标
out vec3 Normal; // 顶点的世界法线
// uniform 是CPU传入的全局值，注意是靠名字关联的，两边名字必须一样。上面 in 的名字可以任意。
uniform mat4 model;
uniform mat3 model3_inverse_transpose;
// uniform mat4 view;
// uniform mat4 projection;
uniform mat4 projection_view;
// 主函数。GLSL里面浮点数字面值默认就是单精度，不需要加后缀f，加了也没事。double要加后缀lf或者L，比如
// double d = 0.5lf;
// 注意GLSL里面乘号*含义很特殊，标量乘向量就是字面意思，向量乘向量是逐元素乘法，dot函数才是点乘。
// 矩阵乘向量或者矩阵都是矩阵乘法。向量乘矩阵会自动把向量当行向量做矩阵乘法。
void main() {
    // vec4(aPos, 1.0) 把三维位置转化成齐次坐标(x, y, z, 1)
    // vec3(model * 齐次坐标) 用model矩阵从局部坐标变换到世界坐标，vec3去掉w（这里恒为1）
    FragPos = vec3(model * vec4(aPos, 1.0));
    // 接下来要做的是法线变换，直接代码如下，公式推导见window.c。
    // Normal = mat3(transpose(inverse(model))) * aNormal;
    // mat3 这里是取 mat4 的左上角3*3。由于model不涉及非仿射投影，最后一行一定是(0,0,0,1)，可以不要。法线平移不影响方向，所以平移部分可以不要。
    // 如果 model 包含非均匀缩放为 0（比如 scale(0, 1, 1)，某方向缩放为 0），矩阵不可逆。
    // 此时 inverse(model) 结果未定义（可能返回垃圾值、NaN，甚至崩溃）。
    // 如果某个轴缩放非常小，接近0但不为0，可能条件数差，数值上求逆会有问题。
    // 很多引擎的做法：在 CPU 端算好法线矩阵 normalMatrix = transpose(inverse(mat3(model)))，作为单独 uniform 传给着色器。
    // 其实transpose(inverse(model)) 和 projection * view都可以CPU算好了一次性传过来，避免每个顶点GPU都得算一次。
    // 不过这些不依赖于顶点信息的内容，编译器可能会做优化到循环外面一次性计算。但不保证。
    Normal = model3_inverse_transpose * aNormal;
    // gl_Position 是内置输出变量，必须设置，表示顶点在裁剪空间的位置。应用完整的MVP 变换，最终 GPU 用它做裁剪 + 透视除法 + 视口变换。
    // gl_Position = projection * view * vec4(FragPos, 1.0); // 合并成了下面一行
    gl_Position = projection_view * vec4(FragPos, 1.0);
}
