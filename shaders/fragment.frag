#version 330 core
in vec3 FragPos; // 输入，来源于顶点着色器插值，世界坐标下的顶点坐标，要和顶点着色器变量名一致
in vec3 Normal; // 输入，来源于顶点着色器插值，世界坐标下的顶点法线，要和顶点着色器变量名一致
out vec4 FragColor; // 输出，片元最终颜色（RGBA），CPU 端对应你设的 objectColor 等
// CPU传来的全局变量，要和CPU部分变量名一致
uniform vec3 lightDir;  // 光的方向
uniform vec3 lightDirComp;  // 光的方向
uniform vec3 lightColor; // 光的强度
uniform vec3 objectColor; // 物体颜色，每个物体不同
uniform vec3 ambientColor; // 环境光颜色
void main() {
    // 环境光 = 环境光强度 × 物体颜色。
    // 给物体一个“兜底亮度”，防止死黑
    vec3 ambient = ambientColor * objectColor;
    // 归一化法线，因为插值后法线长度可能不是1。
    vec3 norm = normalize(Normal);
    // 光方向归一化，这里再保证一下。其实也不用做，只要确定CPU那边做好了归一化就行
    vec3 lightDirNorm = normalize(lightDir);
    // 法线和光方向的点积就是漫反射系数diff。描述表面朝向光的程度，决定“这个表面受光多强”。
    // 但是注意背光面不产生负亮度，所以要取和0.0的较大值。
    float diff = max(dot(norm, lightDirNorm), 0.0);
    // 背面光，相当于打一个反光板
    vec3 lightDirCompNorm = normalize(lightDirComp);
    float diffComp = max(dot(norm, lightDirCompNorm), 0.0);
    // 漫反射 = 漫反射系数 × 光颜色 × 物体颜色。
    vec3 diffuse = (diff + 0.3 * diffComp) * lightColor * objectColor;
    // 最终颜色=环境光+漫反射。
    vec3 result = ambient + diffuse;
    // 输出最终颜色，设为完全不透明，alpha=1。
    FragColor = vec4(result, 1.0);
}
