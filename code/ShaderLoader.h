#ifndef SHADER_LOADER_H
#define SHADER_LOADER_H



/*
    Shader loader Class reads our shader programs from the file, 
    compiles them links them together in a Shader Program,
    the Program ID is stored in the class.
*/
#include "..\lib\glad\glad.h"  

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

struct ShaderLoader
{
    /* data */

    //Program Id
    unsigned int shaderID;

    ShaderLoader();
    /*
        W
    */
    void Init(const char* vertexPath, const char* fragmentPath);

    //use/activate the shader
    void use();

    //utility uniform functions
    void setUniBool(const std::string &name, bool value) const;
    void setUniInt(const std::string &name, int value) const;
    void setUniFloat(const std::string &name, float value) const;
};
 

#endif