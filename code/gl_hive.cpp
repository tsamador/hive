


#include "..\lib\glad\glad.h"           
#include "..\lib\GLFW\glfw3.h"
#include "gl_hive.h"
#include "ShaderLoader.h"
//#include "stb_image.h"
#include <stdio.h> //NOTE(Tanner): May want to eventually remove this.

ShaderLoader shaders;

int main()
{

    //Setup 
    // Initialize and cvonfigure glfw
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    //
    GLFWwindow* window = glfwCreateWindow(800,600,"hive",0,0);

    if(!window)
    {
        puts("Failed to create GLFW Window\n");
        glfwTerminate();
        return -1;
    }
    
    glfwMakeContextCurrent(window);

    //NOTE(Tanner): Assign the callback function For when the window is resized
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    //Initialize GlAD
    if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        puts("Failed to Initalize GLAD\n");
        return -1;
    }

    

}

/*
    Callback function for when the window is resized.  
*/
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0,0, width, height);
}

/*
    NOTE(Tanner): This may return something in the future, likely a struct representing
    everything the game needs to know about the game window

    Function to setup and configure OpenGl
*/
void gl_ConfigureOpenGl()
{

}