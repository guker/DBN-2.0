//
//  Viz.cpp
//  DBN
//
//  Created by Devon Hjelm on 7/24/12.
//  Copyright 2012 __MyCompanyName__. All rights reserved.
//
#include "Viz.h"
#include "Layers.h"
#include "Connections.h"
#include "DBN.h"
#include "IO.h"
#include "DBN.h"
#include "Monitors.h"
#include "Viz_Units.h"

#define STRINGIFY_HELPER(str) #str
#define STRINGIFY(str) STRINGIFY_HELPER(str)

//BEGIN OPENGL stuff

bool   _running;             //< true if the program is running, false if it is time to terminate
Visualizer *the_viz;
Monitor *the_monitor;

//--------------------------------------------------------------

void Visualizer::terminate(int exitCode)
{
   // Delete vertex buffer object
   if(_vertexBuf)
   {
      glDeleteBuffers(1, &_vertexBuf);
      _vertexBuf = 0;
   }
   
   // Delete vertex buffer object
   if(_texCoordBuf)
   {
      glDeleteBuffers(1, &_texCoordBuf);
      _texCoordBuf = 0;
   }
   
   // Delete vertex array object
   if(_vao)
   {
      glDeleteVertexArrays(1, &_vao);
   }
   
   // Delete shader program
   if(_program)
   {
      glDeleteProgram(_program);
   }
   
   glfwTerminate();
   
   exit(exitCode);
}

/**
 * Creates a string by reading a text file.
 *
 * @param filename	The name of the file
 * @return			A string that contains the contents of the file
 */
std::string Visualizer::readTextFile(const std::string& filename)
{
   std::ifstream infile(filename.c_str()); // File stream
   std::string source;                     // Text file string
   std::string line;                       // A line in the file
   
   // Make sure the file could be opened
   if(!infile.is_open())
   {
      std::cerr << "Could not open file: " << filename << std::endl;
      terminate(EXIT_FAILURE);
   }
   
   // Read in the source one line at a time, then append it
   // to the source string. Not efficient.
   while(infile.good())
   {
      getline(infile, line);
      source = source + line + "\n";
   }
   
   infile.close();
   return source;
}

/**
 * Check the compile status of a shader
 *
 * @param shader     Handle to a shader
 * @return           true if the shader was compiled, false otherwise
 */
bool Visualizer::shaderCompileStatus(GLuint shader)
{
   GLint compiled;
   glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
   return compiled ? true : false;
}

/**
 * Retrieve a shader log
 *
 * @param shader     Handle to a shader
 * @return           The contents of the log
 */
std::string Visualizer::getShaderLog(GLuint shader)
{
   // Get the size of the log and allocate the required space
   GLint size;
   glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &size);
   
   // Allocate space for the log
   char* log = new char[size];
   
   // Get the shader log
   glGetShaderInfoLog(shader, size, NULL, log);
   
   // Convert it into a string (not efficient)
   std::string retval(log);
   
   // Free up space
   delete [] log;
   
   return retval;
}

/**
 * Check the link status of a program
 *
 * @param shader     Handle to a shader
 * @return           true if the shader was compiled, false otherwise
 */
bool Visualizer::programLinkStatus(GLuint program)
{
   GLint linked;
   glGetProgramiv(_program, GL_LINK_STATUS, &linked);
   return linked ? true : false;
}

/**
 * Retrieve a GLSL program log
 *
 * @param shader     Handle to a GLSL program
 * @return           The contents of the log
 */
std::string Visualizer::getProgramLog(GLuint program)
{
   // Get the size of the log and allocate the required space
   GLint size;
   glGetProgramiv(_program, GL_INFO_LOG_LENGTH, &size);
   char* log = new char[size];
   
   // Get the program log
   glGetProgramInfoLog(program, size, NULL, log);
   
   // Convert it into a string (blah)
   std::string retval(log);
   
   // Clean up and return
   delete [] log;
   return retval;
}

/**
 * Create a shader program from a source string. The caller should
 * check the compile status
 *
 * @param source        The shader source
 * @param shaderType    The type of shader (GL_VERTEX_SHADER, etc)
 * @return              A handle to the shader program. 0 if an
 *                      error occured.
 */
GLuint Visualizer::createShader(const std::string& source, GLenum shaderType)
{
   GLuint shader = glCreateShader(shaderType);
   
   const GLchar* sourcePtr0 = source.c_str();
   const GLchar** sourcePtr = &sourcePtr0;
   
   // Set the source and attempt compilation
   glShaderSource(shader, 1, sourcePtr, NULL);
   glCompileShader(shader);
   
   return shader;
}

/**
 * Create a GLSL program object from vertex and fragment shader files.
 *
 * @param  vShaderFile   The vertex shader filename
 * @param  fShaderFile   The fragment shader filename
 * @return handle to the GLSL program
 */
GLuint Visualizer::createGLSLProgram()
{
   std::string vertexSource = readTextFile(vertexPath);
   std::string fragmentSource = readTextFile(fragmentPath);

   
   _program = glCreateProgram();
   
   // Create vertex shader
   GLuint vertexShader  = createShader(vertexSource, GL_VERTEX_SHADER);
   
   // Check for compile errors
   if(!shaderCompileStatus(vertexShader))
   {
      std::cerr << "Could not compile " << vertexShader << std::endl;
      std::cerr << getShaderLog(vertexShader) << std::endl;
      terminate(EXIT_FAILURE);
   }
   
   // Create fragment shader
   GLuint fragmentShader = createShader(fragmentSource, GL_FRAGMENT_SHADER);
   
   // Check for compile errors
   if(!shaderCompileStatus(fragmentShader))
   {
      std::cerr << "Could not compile " << vertexShader << std::endl;
      std::cerr << getShaderLog(fragmentShader) << std::endl;
      terminate(EXIT_FAILURE);
   }
   
   // Attach the shaders to the program
   glAttachShader(_program, vertexShader);
   glAttachShader(_program, fragmentShader);
   
   // Link the program
   glLinkProgram(_program);
   
   // Check for linker errors
   if(!programLinkStatus(_program))
   {
      std::cerr << "GLSL program failed to link:" << std::endl;
      std::cerr << getProgramLog(_program) << std::endl;
      terminate(EXIT_FAILURE);
   }
   
   return _program;
}

/**
 * Initialize vertex array objects, vertex buffer objects,
 * clear color and depth clear value
 */
void Visualizer::init(int num_tex_maps, int num_line_plots)
{
#ifndef __APPLE__
   // GLEW has trouble supporting the core profile
   glewExperimental = GL_TRUE;
   glewInit();
   if(!GLEW_ARB_vertex_array_object)
   {
      std::cerr << "ARB_vertex_array_object not available." << std::endl;
      terminate(EXIT_FAILURE);
   }
#endif
   // Vertices for the textured quad
   _points.push_back(glm::vec4(-1, -1, 0, 1));
   _points.push_back(glm::vec4(-1,  1, 0, 1));
   _points.push_back(glm::vec4( 1, -1, 0, 1));
   _points.push_back(glm::vec4( 1,  1, 0, 1));
   
   // Texture coordinates for the textured quad
   _texCoords.push_back(glm::vec2(0,1));
   _texCoords.push_back(glm::vec2(0,0));
   _texCoords.push_back(glm::vec2(1,1));
   _texCoords.push_back(glm::vec2(1,0));
   
   // Create the shader program
   _program = createGLSLProgram();
   // Use the shader program that was loaded, compiled and linked
   glUseProgram(_program);
   
   
   // Generate a single handle for a vertex array
   glGenVertexArrays(1, &_vao);
   
   // Bind that vertex array
   glBindVertexArray(_vao);
   
   // Get the location of the "vertex" attribute in the shader program
   _colorLocation = glGetUniformLocation(_program, "color");
   _vertexLocation = glGetAttribLocation(_program, "vertex");
   _texCoordLocation = glGetAttribLocation(_program, "texCoord");
   _weightSamplerLoc = glGetUniformLocation(_program, "weight");
   _mvpLocation = glGetUniformLocation(_program, "mvp");
   _colorFilter = glGetUniformLocation(_program, "colorFilter");
   
   // Generate one handle for the vertex buffer object
   glGenBuffers(1, &_vertexBuf);
   
   // Make that vbo the current array buffer. Subsequent array buffer operations
   // will affect this vbo
   //
   // It is possible to place all data into a single buffer object and use
   // offsets to tell OpenGL where the data for a vertex array or any other
   // attribute may reside.
   glBindBuffer(GL_ARRAY_BUFFER, _vertexBuf);
   
   // Set the data for the vbo. This will load it onto the GPU
   glBufferData(GL_ARRAY_BUFFER,                // Target buffer object
                sizeof(glm::vec4) * _points.size(),  // Size in bytes of the buffer
                (GLfloat*) &_points[0],         // Pointer to the data
                GL_STATIC_DRAW);                // Expected data usage pattern
   
   // Specify the location and data format of the array of generic vertex attributes
   glVertexAttribPointer(_vertexLocation, // Attribute location in the shader program
                         4,               // Number of components per attribute
                         GL_FLOAT,        // Data type of attribute
                         GL_FALSE,        // GL_TRUE: values are normalized or
                         // GL_FALSE: values are converted to fixed point
                         0,               // Stride
                         0);              // Offset into VBO for this data
   
   // Enable the generic vertex attribute array
   glEnableVertexAttribArray(_vertexLocation);
   
   glGenBuffers(1, &_texCoordBuf);
   glBindBuffer(GL_ARRAY_BUFFER, _texCoordBuf);
   // Set the data for the vbo. This will load it onto the GPU
   glBufferData(GL_ARRAY_BUFFER,                // Target buffer object
                sizeof(glm::vec2) * _texCoords.size(),  // Size in bytes of the buffer
                (GLfloat*) &_texCoords[0],         // Pointer to the data
                GL_STATIC_DRAW);                // Expected data usage pattern
   
   // Specify the location and data format of the array of generic vertex attributes
   glVertexAttribPointer(_texCoordLocation, // Attribute location in the shader program
                         2,               // Number of components per attribute
                         GL_FLOAT,        // Data type of attribute
                         GL_FALSE,        // GL_TRUE: values are normalized or
                         // GL_FALSE: values are converted to fixed point
                         0,               // Stride
                         0);              // Offset into VBO for this data
   
   // Enable the generic vertex attribute array
   glEnableVertexAttribArray(_texCoordLocation);
   
   // Set up texture object
   
   GLsizei _texWidth = 256;
   GLsizei _texHeight = 256;
   
   GLfloat texels[_texWidth][_texHeight][4];
   
   // Create a checkerboard pattern
   for(int i = 0; i < _texWidth; i++ )
   {
      for(int j = 0; j < _texHeight; j++ )
      {
         GLubyte c = (((i & 0x8) == 0) ^ ((j & 0x8)  == 0)) * 255;
         texels[i][j][0] = c / (255.0f * 1.5f);
         texels[i][j][1] = 0;
         texels[i][j][2] = c / 255.0f;
         texels[i][j][3] = 1.0f;
      }
   }
   
   _texID.resize(num_tex_maps);
   glGenTextures(num_tex_maps, &_texID[0]);
   
   for(size_t i = 0; i < _texID.size(); ++i)
   {
      glBindTexture(GL_TEXTURE_2D, _texID[i]);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, _texWidth, _texHeight, 0, GL_RGBA, GL_FLOAT, texels);
      GL_ERR_CHECK();
      glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
      GL_ERR_CHECK();
      glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
      GL_ERR_CHECK();
      glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST ); // GL_NEAREST for no interpolation, GL_LINEAR for interpolation
      GL_ERR_CHECK();
      glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
      GL_ERR_CHECK();
      glBindTexture(GL_TEXTURE_2D, 0);
   }
   
   // Generate VAO and buffer objects for line plots
   _lineVAO.resize(num_line_plots);
   _lineBO.resize(num_line_plots);
   glGenVertexArrays(GLsizei(_lineVAO.size()), &_lineVAO[0]);
   glGenBuffers(GLsizei(_lineBO.size()), &_lineBO[0]);
   
   // Set the clear color
   //glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
   
   // Set the depth clearing value
   glClearDepth(1.0f);
   
   glPointSize(10.0f);
}


void Visualizer::resize_maps(int num_tex_maps, int num_line_plots) {
   
   GLsizei _texWidth = 256;
   GLsizei _texHeight = 256;
   
   GLfloat texels[_texWidth][_texHeight][4];
   
   // Create a checkerboard pattern
   for(int i = 0; i < _texWidth; i++ )
   {
      for(int j = 0; j < _texHeight; j++ )
      {
         GLubyte c = (((i & 0x8) == 0) ^ ((j & 0x8)  == 0)) * 255;
         texels[i][j][0] = c / (255.0f * 1.5f);
         texels[i][j][1] = 0;
         texels[i][j][2] = c / 255.0f;
         texels[i][j][3] = 1.0f;
      }
   }
   glDeleteTextures((int)_texID.size(), &_texID[0]);
   _texID.resize(num_tex_maps);
   glGenTextures(num_tex_maps, &_texID[0]);
   
   for(size_t i = 0; i < _texID.size(); ++i)
   {
      glBindTexture(GL_TEXTURE_2D, _texID[i]);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, _texWidth, _texHeight, 0, GL_RGBA, GL_FLOAT, texels);
      GL_ERR_CHECK();
      glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
      GL_ERR_CHECK();
      glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
      GL_ERR_CHECK();
      glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST ); // GL_NEAREST for no interpolation, GL_LINEAR for interpolation
      GL_ERR_CHECK();
      glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
      GL_ERR_CHECK();
      glBindTexture(GL_TEXTURE_2D, 0);
   }
   
   glDeleteVertexArrays((int)_lineVAO.size(), &_lineVAO[0]);
   glDeleteVertexArrays((int)_lineBO.size(), &_lineBO[0]);
   // Generate VAO and buffer objects for line plots
   _lineVAO.resize(num_line_plots);
   _lineBO.resize(num_line_plots);
   glGenVertexArrays(GLsizei(_lineVAO.size()), &_lineVAO[0]);
   glGenBuffers(GLsizei(_lineBO.size()), &_lineBO[0]);

}

/**
 * Window resize callback
 *
 * @param width   the width of the window
 * @param height  the height of the window
 */
void GLFWCALL resize(int width, int height)
{
   // Set the affine transform of (x,y) from normalized device coordinates to
   // window coordinates. In this case, (-1,1) -> (0, width) and (-1,1) -> (0, height)
   glViewport(0, 0, width, height);
}

/**
 * Keypress callback
 */
void GLFWCALL keypress(int key, int state)
{
   if(state == GLFW_PRESS)
   {
      switch(key)
      {
         case GLFW_KEY_ESC:
            _running = false;
            break;
         case 'V' :
            the_viz->toggle_on();
            break;
         case GLFW_KEY_SPACE :
            the_viz->toggle_pause();
            break;
         case 'L' :
            the_monitor->send_stop_signal();
            break;
         case '+' :
         case '=' :
            the_monitor->teacher->multiply_rate();
            break;
         case '-' :
            the_monitor->teacher->divide_rate();
            break;
         case '0' :
            the_monitor->teacher->learning_multiplier = 1;
            break;
         case '[' :
            if (the_monitor->threshold > 0) the_monitor->threshold -=.1;
            break;
         case ']' :
            if (the_monitor->threshold < 1) the_monitor->threshold +=.1;
            break;
         case '<' :
         case ',' :
            the_monitor->move_down_stack();
            break;
         case '>' :
         case '.' :
            the_monitor->move_up_stack();
            break;
      }
   }
}

/**
 * Window close callback
 */
int GLFWCALL close(void)
{
   _running = false;
   return GL_TRUE;
}

/**
 * Main loop
 * @param time    time elapsed in seconds since the start of the program
 */


int Visualizer::draw_texture_map(Tex_Unit *tex_unit, int id)
{
   glBindTexture(GL_TEXTURE_2D, _texID[id]);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, (int)tex_unit->viz_matrix->size2, (int)tex_unit->viz_matrix->size1, 0, GL_RED, GL_FLOAT, tex_unit->viz_matrix->data);
   //GL_ERR_CHECK();
   glm::mat4 mvp;
   
   mvp = glm::translate(glm::mat4(), glm::vec3(.1*tex_unit->x_position, .1*tex_unit->y_position, .1*tex_unit->z_position)) * glm::scale(glm::mat4(), glm::vec3(.1*tex_unit->x_size, .1*tex_unit->y_size, .1*tex_unit->z_size));
   
   glUseProgram(_program);
   glBindVertexArray(_vao);
   
   glActiveTexture(GL_TEXTURE0);
   glUniformMatrix4fv(_mvpLocation, 1, GL_FALSE, glm::value_ptr(mvp));
   glUniform4fv(_colorLocation, 1, tex_unit->color);
   glUniform1i(_colorFilter, 1);
   glDrawArrays(GL_TRIANGLE_STRIP, 0, (GLsizei)_points.size());
   return 1;
}

int Visualizer::draw_plot(Plot_Unit *plot_unit, int id){
   
   // Draw some line plots
   std::vector<glm::vec4> _lineData(0);
   int x = 0;
   
   for (int i = 0; i < plot_unit->line_set->size; ++i){
      float y = gsl_vector_float_get(plot_unit->line_set, i);
      if (y==0) break;
      _lineData.push_back(glm::vec4(x,y,0,1));
      ++x;
   }
   
   glUseProgram(_program);
   glBindVertexArray(_lineVAO[id]);
   glBindBuffer(GL_ARRAY_BUFFER, _lineBO[id]);
   // copy data to gpu
   glBufferData(GL_ARRAY_BUFFER, _lineData.size() * sizeof(glm::vec4), &_lineData[0], GL_STATIC_DRAW);
   // Tell OpenGL where the beginning of the buffer starts for this attribute
   glVertexAttribPointer(_vertexLocation, 4, GL_FLOAT, GL_FLOAT, 0, NULL);
   
   float line_min, line_max;
   gsl_vector_float_minmax(plot_unit->line_set, &line_min, &line_max);
   
   glm::mat4 mvp = glm::translate(glm::mat4(), glm::vec3(.1*(plot_unit->x_position-plot_unit->x_size), .1*plot_unit->y_position, .1*plot_unit->z_position)) * glm::scale(glm::mat4(), glm::vec3(.2*plot_unit->x_size/plot_unit->line_set->size, .1*plot_unit->y_size/(line_max-line_min), plot_unit->z_size));
   glUniformMatrix4fv(_mvpLocation, 1, GL_FALSE, glm::value_ptr(mvp));
   float color[] = {1,0,0,1};
   glUniform4fv(_colorLocation, 1, color);
   glUniform1i(_colorFilter, 0);
   // Enable the generic vertex attribute array
   glEnableVertexAttribArray(_vertexLocation);
   glDrawArrays(GL_LINE_STRIP, 0, GLsizei(_lineData.size()));
   return 1;
}

int Visualizer::draw_border(Border *border, int id){
   
   // Draw some line plots
   std::vector<glm::vec4> _lineData(0);
   
   float x = 0;
   float y = 0;
   float dx = border->x_size;
   float dy = border->y_size;
   
   _lineData.push_back(glm::vec4(x-dx, y-dy,0,1));
   _lineData.push_back(glm::vec4(x-dx, y+dy,0,1));
   _lineData.push_back(glm::vec4(x+dx, y+dy,0,1));
   _lineData.push_back(glm::vec4(x+dx, y-dy,0,1));
   _lineData.push_back(glm::vec4(x-dx, y-dy,0,1));
   
   glUseProgram(_program);
   glBindVertexArray(_lineVAO[id]);
   glBindBuffer(GL_ARRAY_BUFFER, _lineBO[id]);
   // copy data to gpu
   glBufferData(GL_ARRAY_BUFFER, _lineData.size() * sizeof(glm::vec4), &_lineData[0], GL_STATIC_DRAW);
   // Tell OpenGL where the beginning of the buffer starts for this attribute
   glVertexAttribPointer(_vertexLocation, 4, GL_FLOAT, GL_FLOAT, 0, NULL);
   
   glm::mat4 mvp = glm::translate(glm::mat4(), glm::vec3(.1*border->x_position, .1*border->y_position, .1*border->z_position)) * glm::scale(glm::mat4(), glm::vec3(.1, .1, border->z_size));
   glUniformMatrix4fv(_mvpLocation, 1, GL_FALSE, glm::value_ptr(mvp));
   float color[] = {1,0,0,1};
   glUniform4fv(_colorLocation, 1, color);
   
   glUniform1i(_colorFilter, 0);
   // Enable the generic vertex attribute array
   glEnableVertexAttribArray(_vertexLocation);
   glDrawArrays(GL_LINE_STRIP, 0, GLsizei(_lineData.size()));
   return 1;
}

//END OPENGL stuff

int Visualizer::update(Monitor *monitor){
   if (!on) {glfwWaitEvents(); return 1;}
   while (pause) {glfwWaitEvents();}
   glfwGetTime();
   
   glClearColor(1, 1, 1, 1);
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
   
   for (auto texture_map:monitor->tex_units){
      int id = texture_map.first;
      Tex_Unit *t_unit = texture_map.second;
      t_unit->scale_matrix_and_threshold(monitor);
      draw_texture_map(t_unit, id);
   }
   for (auto line_map:monitor->plots){
      int id = line_map.first;
      Plot_Unit *l_unit = line_map.second;
      draw_plot(l_unit, id);
   }
   
   for (auto border_map:monitor->borders){
      int id = border_map.first;
      Border *b_unit = border_map.second;
      draw_border(b_unit, id);
   }
   
   glfwSwapBuffers();
   
   glUseProgram(0);
   
   return GL_TRUE;
}

void Visualizer::open_window(int width, int height){
   
   // Initialize GLFW
   _running = true;
   on = true;
   glfwInit();
   
   // Request an OpenGL core profile context, without backwards compatibility
   glfwOpenWindowHint(GLFW_OPENGL_VERSION_MAJOR,  3);
   glfwOpenWindowHint(GLFW_OPENGL_VERSION_MINOR,  2);
   glfwOpenWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
   glfwOpenWindowHint(GLFW_OPENGL_PROFILE,        GLFW_OPENGL_CORE_PROFILE);
   
   // Open a window and create its OpenGL context
   if(!glfwOpenWindow(width, height, 0,0,0,0, 32,0, GLFW_WINDOW ))
   {
      std::cerr << "Failed to open GLFW window" << std::endl;
      glfwTerminate();
   }
   
   glfwSetWindowSizeCallback(resize);
   glfwSetKeyCallback(keypress);
   glfwSetWindowCloseCallback(close);
   
}

void Visualizer::close_window(){
   GLFWCALL close();
}






