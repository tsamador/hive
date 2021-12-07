#include "ShaderLoader.h"


/*
    NOTE(Tanner): Our constructor is empty, this is because we can't enforce
    what order our global objects are initialized in c++, and thus don't know 
    what has been initialized. The actually construction work takes place
    in the Init() function
*/
ShaderLoader::ShaderLoader(){}

void ShaderLoader::Init(const char* vertexPath, const char* fragmentPath)
{
        std::string vertexSource = "";
    std::string fragmentSource = "";

    std::ifstream vertexIn;
    std::ifstream fragmentIn;

    
    vertexIn.open(vertexPath);

    if(vertexIn.good())
    {
        std::stringstream buffer;
        buffer << vertexIn.rdbuf();
        vertexSource = buffer.str();
        vertexIn.close();
    }
    else
    {
        printf("Failed to open VertexShader file\n");
    }    

    fragmentIn.open(fragmentPath);

    if(fragmentIn.good())
    {
        std::stringstream buffer;
        buffer << fragmentIn.rdbuf();
        fragmentSource = buffer.str();
        //printf("%s\n", fragmentSource.c_str());
        fragmentIn.close();
    }
    else
    {
        printf("Failed to open FragmentShader file\n");
    }

    //Now compile our shaders
    unsigned int vertex, fragment;
    int success;
    char infoLog[512];

    vertex = glCreateShader(GL_VERTEX_SHADER);
    const char* vertexSourceCString = vertexSource.c_str();
    //printf("%s\n", vertexSourceCString);
    glShaderSource(vertex, 1, &vertexSourceCString, NULL);
    glCompileShader(vertex);

    //Check to see if we compiled the shader successfully
    glGetShaderiv(vertex, GL_COMPILE_STATUS, &success);

    if(!success)
    {
        glGetShaderInfoLog(vertex,512,NULL, infoLog);
        printf("ERROR::VERTEX::SHADER Compilation failed \n%s", infoLog);
    }

    fragment = glCreateShader(GL_FRAGMENT_SHADER);
    const char* fragmentSourceCString = fragmentSource.c_str();
    //printf("Fragment Shader:\n %s\n", fragmentSourceCString);
    //Attach the source to the shader ID and compile it

    glShaderSource(fragment, 1, &fragmentSourceCString, NULL);
    glCompileShader(fragment);

    //check to see if we compiled the fragmentShader successfully
    glGetShaderiv(fragment, GL_COMPILE_STATUS, &success);

    if(!success)
    {
        glGetShaderInfoLog(fragment,512,NULL, infoLog);
        printf("ERROR::FRAGMENT::SHADER Compilation failed \n%s", infoLog);
    }

    
    shaderID = glCreateProgram();

    glAttachShader(shaderID, vertex);
    glAttachShader(shaderID, fragment);
    glLinkProgram(shaderID);

    glGetProgramiv(shaderID, GL_LINK_STATUS, &success);

    if(!success)
    {
        glGetProgramInfoLog(shaderID,512,NULL, infoLog);
        printf("ERROR:SHADER Program Compilation failed \n%s", infoLog);
    }
    
    glDeleteShader(vertex);
    glDeleteShader(fragment);


}

void ShaderLoader::use()
{
    glUseProgram(shaderID);
}

void ShaderLoader::setUniBool(const std::string &name, bool value) const
{
    int loc = glGetUniformLocation(shaderID, name.c_str());
    glUniform1i(loc, (int) value);
}
void ShaderLoader::setUniInt(const std::string &name, int value) const
{
    int loc = glGetUniformLocation(shaderID, name.c_str());
    glUniform1i(loc, value);
}
void ShaderLoader::setUniFloat(const std::string &name, float value) const
{
    int loc = glGetUniformLocation(shaderID, name.c_str());
    glUniform1f(loc, value);
}
