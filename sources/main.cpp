// Include standard headers
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <time.h>
// Include GLEW
#include <GL/glew.h>

// Include GLFW
#include <glfw3.h>
GLFWwindow* g_pWindow;
unsigned int g_nWidth = 1024, g_nHeight = 768;

// Include AntTweakBar
#include <AntTweakBar.h>
TwBar *g_pToolBar;

// Include GLM
#include <glm/gtx/constants.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>
using namespace glm;

#include <shader.hpp>
#include <texture.hpp>
#include <controls.hpp>
#include <objloader.hpp>
#include <vboindexer.hpp>
#include <glerror.hpp>

void WindowSizeCallBack(GLFWwindow *pWindow, int nWidth, int nHeight) {

	g_nWidth = nWidth;
	g_nHeight = nHeight;
	glViewport(0, 0, g_nWidth, g_nHeight);
	TwWindowSize(g_nWidth, g_nHeight);
}
struct model {
	std::vector<unsigned short> indices;
	std::vector<glm::vec3> indexed_vertices;
	std::vector<glm::vec2> indexed_uvs;
	std::vector<glm::vec3> indexed_normals;
	GLuint MatrixID, ViewMatrixID, ModelMatrixID, TextureID, VertexArrayID;
	GLuint vertexbuffer, uvbuffer, normalbuffer, elementbuffer, Texture;
	glm::mat4 MVP, ProjectionMatrix, ViewMatrix, ModelMatrix = glm::mat4(1.0f);
};

struct camera {
	glm::vec3 position;
	glm::vec3 direction;
	glm::vec3 right;
	glm::vec3 up;
	float fov = 45;
	glm::mat4 ProjectionMatrix = glm::perspective(45.0f, 1024.0f / 768.0f, 0.1f, 100.f);
	glm::mat4 ViewMatrix;
};

bool checkCollision(struct model * a, struct model * b) {
	float threshold = 1.5f;
	return !(
		(a->ModelMatrix[3].x + threshold / 2) <= (b->ModelMatrix[3].x - threshold / 2) ||
		(a->ModelMatrix[3].x - threshold / 2) >= (b->ModelMatrix[3].x + threshold / 2) ||
		(a->ModelMatrix[3].y + threshold / 2) <= (b->ModelMatrix[3].y - threshold / 2) ||
		(a->ModelMatrix[3].y - threshold / 2) >= (b->ModelMatrix[3].y + threshold / 2)   );
}

int main(void)
{
	int nUseMouse = 0;
	float hAngle = glm::pi<float>();
	float vAngle = 0.0f;
	// Initialise GLFW
	if (!glfwInit())
	{
		fprintf(stderr, "Failed to initialize GLFW\n");
		return -1;
	}

	glfwWindowHint(GLFW_SAMPLES, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// Open a window and create its OpenGL context
	g_pWindow = glfwCreateWindow(g_nWidth, g_nHeight, "CG UFPel", NULL, NULL);
	if (g_pWindow == NULL) {
		fprintf(stderr, "Failed to open GLFW window. If you have an Intel GPU, they are not 3.3 compatible. Try the 2.1 version of the tutorials.\n");
		glfwTerminate();
		return -1;
	}

	glfwMakeContextCurrent(g_pWindow);

	// Initialize GLEW
	glewExperimental = true; // Needed for core profile
	if (glewInit() != GLEW_OK) {
		fprintf(stderr, "Failed to initialize GLEW\n");
		return -1;
	}

	check_gl_error();//OpenGL error from GLEW

	// Initialize the GUI
	TwInit(TW_OPENGL_CORE, NULL);
	TwWindowSize(g_nWidth, g_nHeight);

	// Set GLFW event callbacks. I removed glfwSetWindowSizeCallback for conciseness
	glfwSetMouseButtonCallback(g_pWindow, (GLFWmousebuttonfun)TwEventMouseButtonGLFW); // - Directly redirect GLFW mouse button events to AntTweakBar
	glfwSetCursorPosCallback(g_pWindow, (GLFWcursorposfun)TwEventMousePosGLFW);          // - Directly redirect GLFW mouse position events to AntTweakBar
	glfwSetScrollCallback(g_pWindow, (GLFWscrollfun)TwEventMouseWheelGLFW);    // - Directly redirect GLFW mouse wheel events to AntTweakBar
	glfwSetKeyCallback(g_pWindow, (GLFWkeyfun)TwEventKeyGLFW);                         // - Directly redirect GLFW key events to AntTweakBar
	glfwSetCharCallback(g_pWindow, (GLFWcharfun)TwEventCharGLFW);                      // - Directly redirect GLFW char events to AntTweakBar
	glfwSetWindowSizeCallback(g_pWindow, WindowSizeCallBack);

	//create the toolbar
	g_pToolBar = TwNewBar("CG UFPel ToolBar");
	// Add 'speed' to 'bar': it is a modifable (RW) variable of type TW_TYPE_DOUBLE. Its key shortcuts are [s] and [S].
	double speed = 0.0;
	TwAddVarRW(g_pToolBar, "speed", TW_TYPE_DOUBLE, &speed, " label='Rot speed' min=0 max=2 step=0.01 keyIncr=s keyDecr=S help='Rotation speed (turns/second)' ");
	// Add 'bgColor' to 'bar': it is a modifable variable of type TW_TYPE_COLOR3F (3 floats color)
	vec3 oColor(0.0f);
	TwAddVarRW(g_pToolBar, "bgColor", TW_TYPE_COLOR3F, &oColor[0], " label='Background color' ");

	// Ensure we can capture the escape key being pressed below
	glfwSetInputMode(g_pWindow, GLFW_STICKY_KEYS, GL_TRUE);
	glfwSetCursorPos(g_pWindow, g_nWidth / 2, g_nHeight / 2);

	// Dark blue background
	glClearColor(0.0f, 0.0f, 0.4f, 0.0f);

	// Enable depth test
	glEnable(GL_DEPTH_TEST);
	// Accept fragment if it closer to the camera than the former one
	glDepthFunc(GL_LESS);

	// Cull triangles which normal is not towards the camera
	glEnable(GL_CULL_FACE);
	struct model example[2];
	struct camera cameras[4];

	cameras[0].position = glm::vec3(0.0f, 0.0f, 10.0f);
	cameras[0].direction = glm::vec3(0.0f, 0.0f, -1.0f);
	std::cout <<"direction :" <<cameras[0].direction.x<<" "<< cameras[0].direction.y << " "<< cameras[0].direction.z <<"\n" ;
	cameras[0].right = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), cameras[0].direction));
	std::cout << "right :" << cameras[0].right.x << " " << cameras[0].right.y << " " << cameras[0].right.z << "\n";
	cameras[0].up = glm::cross(cameras[0].right, cameras[0].direction);
	std::cout << "up :" << cameras[0].up.x << " " << cameras[0].up.y << " " << cameras[0].up.z << "\n";
	cameras[0].ViewMatrix = glm::lookAt(cameras[0].position, cameras[0].position + cameras[0].direction, cameras[0].up);

	cameras[1].position = glm::vec3(0.0f, 0.0f, 7.0f);
	cameras[1].direction = glm::vec3(0.0f, 0.0f, -1.0f);
	std::cout << "direction :" << cameras[1].direction.x << " " << cameras[1].direction.y << " " << cameras[1].direction.z << "\n";
	cameras[1].right = glm::normalize(glm::cross(glm::vec3(0.0f, -1.0f, 0.0f), cameras[1].direction));
	std::cout << "right :" << cameras[1].right.x << " " << cameras[1].right.y << " " << cameras[1].right.z << "\n";
	cameras[1].up = glm::cross(cameras[1].right, cameras[1].direction);
	std::cout << "up :" << cameras[1].up.x << " " << cameras[1].up.y << " " << cameras[1].up.z << "\n";
	cameras[1].ViewMatrix = glm::lookAt(cameras[1].position, cameras[1].position + cameras[1].direction, cameras[1].up);

	cameras[2].position = glm::vec3(0.0f, 0.0f, 20.0f);
	cameras[2].direction = glm::vec3(0.0f, 0.0f, -1.0f);
	cameras[2].up = glm::vec3(0.0f, -1.0f, 0.0f);
	cameras[2].ViewMatrix = glm::lookAt(cameras[2].position, cameras[2].position + cameras[2].direction, cameras[2].up);

	cameras[3].position = glm::vec3(0.0f, 0.0f, -20.0f);
	cameras[3].direction = glm::vec3(0.0f, 0.0f, 1.0f);
	cameras[3].up = glm::vec3(0.0f, 1.0f, 0.0f);
	cameras[3].ViewMatrix = glm::lookAt(cameras[3].position, cameras[3].position + cameras[3].direction, cameras[3].up);
	for (unsigned int i = 0; i < 2; i++) {
		glGenVertexArrays(1, &example[i].VertexArrayID);
		glBindVertexArray(example[i].VertexArrayID);

	}
	// Create and compile our GLSL program from the shaders
	GLuint programID = LoadShaders("shaders/StandardShading.vertexshader", "shaders/StandardShading.fragmentshader");
	
	// Get a handle for our "MVP" uniform
	for (unsigned int i = 0; i < 2; i++) {
		example[i].MatrixID = glGetUniformLocation(programID, "MVP");
		example[i].ViewMatrixID = glGetUniformLocation(programID, "V");
		example[i].ModelMatrixID = glGetUniformLocation(programID, "M");
	}

	// Load the texture
	example[0].Texture = loadDDS("mesh/uvmap.DDS");
	example[1].Texture = loadDDS("mesh/uvmap.DDS");

	// Get a handle for our "myTextureSampler" uniform
	example[0].TextureID = glGetUniformLocation(programID, "myTextureSampler");
	example[1].TextureID = glGetUniformLocation(programID, "myTextureSampler");


	std::vector<glm::vec3> vertices;
	std::vector<glm::vec2> uvs;
	std::vector<glm::vec3> normals;
	// Load it into a VBO
	for (unsigned int i = 0; i < 2; i++) {
		// Read our .obj file
		bool res = loadOBJ("mesh/suzanne.obj", vertices, uvs, normals);
		if (!res) {
			
		}
		else {
			indexVBO(vertices, uvs, normals, example[i].indices, example[i].indexed_vertices, example[i].indexed_uvs, example[i].indexed_normals);

			glGenBuffers(1, &example[i].vertexbuffer);
			glBindBuffer(GL_ARRAY_BUFFER, example[i].vertexbuffer);
			glBufferData(GL_ARRAY_BUFFER, example[i].indexed_vertices.size() * sizeof(glm::vec3), &example[i].indexed_vertices[i], GL_STATIC_DRAW);

			glGenBuffers(1, &example[i].uvbuffer);
			glBindBuffer(GL_ARRAY_BUFFER, example[i].uvbuffer);
			glBufferData(GL_ARRAY_BUFFER, example[i].indexed_uvs.size() * sizeof(glm::vec2), &example[i].indexed_uvs[i], GL_STATIC_DRAW);

			glGenBuffers(1, &example[i].normalbuffer);
			glBindBuffer(GL_ARRAY_BUFFER, example[i].normalbuffer);
			glBufferData(GL_ARRAY_BUFFER, example[i].indexed_normals.size() * sizeof(glm::vec3), &example[i].indexed_normals[i], GL_STATIC_DRAW);

			// Generate a buffer for the indices as well
			glGenBuffers(1, &example[i].elementbuffer);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, example[i].elementbuffer);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, example[i].indices.size() * sizeof(unsigned short), &example[i].indices[i], GL_STATIC_DRAW);
		}
		
	}
	
	// Get a handle for our "LightPosition" uniform
	glUseProgram(programID);
	//GLuint LightID = glGetUniformLocation(programID, "LightPosition_worldspace");
	GLuint LightID = glGetUniformLocation(programID, "LightPosition_worldspace");
	GLuint LightPower = glGetUniformLocation(programID,"lightPower");
	GLuint LightColor = glGetUniformLocation(programID, "lightColor");
	glm::vec3 lightPos[3];
	lightPos[0] = glm::vec3(4, 0, 0); lightPos[1] = glm::vec3(-4, 0, 0); lightPos[2] = glm::vec3(0, 4, 0);
	glm::vec3 lightColor[3];
	lightColor[0] = glm::vec3(1.0f, 0.2f, 0.2f); lightColor[1] = glm::vec3(0.2f, 1, 0.2f); lightColor[2] = glm::vec3(0.2f, 0.2f, 1);
	GLfloat lightPower[3];
	lightPower[0] = 25.0f; lightPower[1] = 75.0f; lightPower[2] = 45.0f;
	// For speed computation
	srand(time(NULL));
	GLfloat cameraSpeed = 0.05f;
	double lastTime = glfwGetTime();
	double timer = glfwGetTime();
	double cTime = 0;
	double animationTime = 5;
	unsigned int defCamera = 0;
	int nbFrames    = 0;
	GLfloat randomX = 0, randomY = 0, randomX2 = 0, randomY2 = 0;
	randomX = ( rand() % 6 - 3.0f ) / 1000;
	randomY = ( rand() % 6 - 3.0f ) / 1000;
	randomX2 = (rand() % 6 - 3.0f) / 1000;
	randomY2 = (rand() % 6 - 3.0f) / 1000;
	glm::mat4 translation1 = glm::mat4(1.0f);
	glm::mat4 translation2 = glm::mat4(1.0f);
	translation1 = glm::translate(translation1, -0.5f, 0.0f, 0.0f);
	translation2 = glm::translate(translation2, 0.5f, 0.0f, 0.0f);

	do{
        check_gl_error();
		cTime = glfwGetTime();
        //use the control key to free the mouse
		if (glfwGetKey(g_pWindow, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
			nUseMouse = 1;
		else
			nUseMouse = 0;

		// Measure speed
		double currentTime = glfwGetTime();
		nbFrames++;
		if (currentTime - lastTime >= 1.0){ // If last prinf() was more than 1sec ago
			// printf and reset
			printf("%f ms/frame\n", 1000.0 / double(nbFrames));
			nbFrames  = 0;
			lastTime += 1.0;
		}

		// Clear the screen
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Use our shader
		glUseProgram(programID);

		if (glfwGetKey(g_pWindow, GLFW_KEY_UP) == GLFW_PRESS) {
			cameras[defCamera].position += cameraSpeed*cameras[defCamera].direction;
			cameras[defCamera].ViewMatrix = glm::lookAt(cameras[defCamera].position, cameras[defCamera].position + cameras[defCamera].direction, cameras[defCamera].up);
		}
		if (glfwGetKey(g_pWindow, GLFW_KEY_DOWN) == GLFW_PRESS) {
			cameras[defCamera].position -= cameraSpeed*cameras[defCamera].direction;
			cameras[defCamera].ViewMatrix = glm::lookAt(cameras[defCamera].position, cameras[defCamera].position + cameras[defCamera].direction, cameras[defCamera].up);
		}
		if (glfwGetKey(g_pWindow, GLFW_KEY_RIGHT) == GLFW_PRESS) {
			cameras[defCamera].position -= glm::normalize(glm::cross(cameras[defCamera].position, cameras[defCamera].up)) * cameraSpeed;
			cameras[defCamera].ViewMatrix = glm::lookAt(cameras[defCamera].position, cameras[defCamera].position + cameras[defCamera].direction, cameras[defCamera].up);
		}
		if (glfwGetKey(g_pWindow, GLFW_KEY_LEFT) == GLFW_PRESS) {
			cameras[defCamera].position += glm::normalize(glm::cross(cameras[defCamera].position, cameras[defCamera].up)) * cameraSpeed;
			cameras[defCamera].ViewMatrix = glm::lookAt(cameras[defCamera].position, cameras[defCamera].position + cameras[defCamera].direction, cameras[defCamera].up);
		}
		if (glfwGetKey(g_pWindow, GLFW_KEY_P) == GLFW_PRESS) {
			defCamera = 3;
		}
		if (glfwGetKey(g_pWindow, GLFW_KEY_L) == GLFW_PRESS) {
			defCamera = 2;
		}
		if (glfwGetKey(g_pWindow, GLFW_KEY_K) == GLFW_PRESS) {
			defCamera = 1;
		}
		if (glfwGetKey(g_pWindow, GLFW_KEY_J) == GLFW_PRESS) {
			defCamera = 0;
		}
		if (glfwGetKey(g_pWindow, GLFW_KEY_W) == GLFW_PRESS) {
			cameras[defCamera].up += glm::vec3(0.0f, 1.0f, 0.0f);
			cameras[defCamera].ViewMatrix = glm::lookAt(cameras[defCamera].position, cameras[defCamera].position + cameras[defCamera].direction, cameras[defCamera].up);
		}
		if (glfwGetKey(g_pWindow, GLFW_KEY_S) == GLFW_PRESS) {
			cameras[defCamera].up += glm::vec3(0.0f, -1.0f, 0.0f);
			cameras[defCamera].ViewMatrix = glm::lookAt(cameras[defCamera].position, cameras[defCamera].position + cameras[defCamera].direction, cameras[defCamera].up);
		}
		// Compute the MVP matrix from keyboard and mouse input
		computeMatricesFromInputs(nUseMouse, g_nWidth, g_nHeight);
		float th = float(cTime / (animationTime + timer) );
		std::cout << th << "\n";
		if (th<1) {
			if (checkCollision(&example[0], &example[1])) {
				std::cout << "Collision!!\n";
				translation1 = translate(translation1, -2.0f, 0.0f, 0.0f);
				translation2 = translate(translation2, 2.0f, 0.0f, 0.0f);
				example[0].ModelMatrix = translation1;
				example[1].ModelMatrix = translation2;
				example[0].MVP = example[0].ProjectionMatrix * example[0].ViewMatrix * example[0].ModelMatrix;
				example[1].MVP = example[1].ProjectionMatrix * example[1].ViewMatrix * example[1].ModelMatrix;
			}
			else {
				example[0].ProjectionMatrix = cameras[defCamera].ProjectionMatrix;
				example[0].ViewMatrix = cameras[defCamera].ViewMatrix;
				translation1 = glm::translate(translation1, randomX*th, randomY*th, 0.0f);
				example[0].ModelMatrix = translation1;
				example[0].MVP = example[0].ProjectionMatrix * example[0].ViewMatrix * example[0].ModelMatrix;
				example[1].ProjectionMatrix = cameras[defCamera].ProjectionMatrix;
				example[1].ViewMatrix = cameras[defCamera].ViewMatrix;
				translation2 = glm::translate(translation2, randomX2*th, randomY2*th, 0.0f);
				example[1].ModelMatrix = translation2;
				example[1].MVP = example[1].ProjectionMatrix * example[1].ViewMatrix * example[1].ModelMatrix;

			}
		}
		else {
			example[0].ModelMatrix = glm::mat4(1.0f) * translation1;
			example[1].ModelMatrix = glm::mat4(1.0f) * translation2;
			randomX = (rand() % 6 - 3.0f) / 1000;
			randomY = (rand() % 6 - 3.0f) / 1000;
			randomX2 = (rand() % 6 - 3.0f) / 1000;
			randomY2 = (rand() % 6 - 3.0f) / 1000;
			//std::cout << "randomXYZ :" << randomX << " " << randomY << " " << randomZ << "\n";
			//std::cout << "randomXYZ2 :" << randomX2 << " " << randomY2 << " " << randomZ2 << "\n";
			timer = cTime + 5.0f;
		}	// Send our transformation to the currently bound shader,
			// in the "MVP" uniform
		
		//glUniform3f(LightID, lightPos.x, lightPos.y, lightPos.z);
		glUniform3fv(LightID, 3, glm::value_ptr(lightPos[0]));
		glUniform1fv(LightPower, 3, &lightPower[0]);
		glUniform3fv(LightColor, 3, glm::value_ptr(lightColor[0]));
		for (unsigned int i = 0; i < 2; i++) {
			glUniformMatrix4fv(example[i].MatrixID, 1, GL_FALSE, &example[i].MVP[0][0]);
			glUniformMatrix4fv(example[i].ModelMatrixID, 1, GL_FALSE, &example[i].ModelMatrix[0][0]);
			glUniformMatrix4fv(example[i].ViewMatrixID, 1, GL_FALSE, &example[i].ViewMatrix[0][0]);

			// Bind our texture in Texture Unit 0
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, example[i].Texture);
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
			// Set our "myTextureSampler" sampler to user Texture Unit 0
			glUniform1i(example[i].TextureID, 0);

			// 1rst attribute buffer : vertices
			glEnableVertexAttribArray(0);
			glBindBuffer(GL_ARRAY_BUFFER, example[i].vertexbuffer);
			glVertexAttribPointer(
				0,                  // attribute
				3,                  // size
				GL_FLOAT,           // type
				GL_FALSE,           // normalized?
				0,                  // stride
				(void*)0            // array buffer offset
			);

			// 2nd attribute buffer : UVs
			glEnableVertexAttribArray(1);
			glBindBuffer(GL_ARRAY_BUFFER, example[i].uvbuffer);
			glVertexAttribPointer(
				1,                                // attribute
				2,                                // size
				GL_FLOAT,                         // type
				GL_FALSE,                         // normalized?
				0,                                // stride
				(void*)0                          // array buffer offset
			);

			// 3rd attribute buffer : normals
			glEnableVertexAttribArray(2);
			glBindBuffer(GL_ARRAY_BUFFER, example[i].normalbuffer);
			glVertexAttribPointer(
				2,                                // attribute
				3,                                // size
				GL_FLOAT,                         // type
				GL_FALSE,                         // normalized?
				0,                                // stride
				(void*)0                          // array buffer offset
			);

			// Index buffer
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, example[i].elementbuffer);

			// Draw the triangles !
			glDrawElements(
				GL_TRIANGLES,        // mode
				example[i].indices.size(),      // count
				GL_UNSIGNED_SHORT,   // type
				(void*)0             // element array buffer offset
			);

			glDisableVertexAttribArray(0);
			glDisableVertexAttribArray(1);
			glDisableVertexAttribArray(2);

		}
		
		// Draw tweak bars
		TwDraw();

		// Swap buffers
		glfwSwapBuffers(g_pWindow);
		glfwPollEvents();

	} // Check if the ESC key was pressed or the window was closed
	while (glfwGetKey(g_pWindow, GLFW_KEY_ESCAPE) != GLFW_PRESS &&
	glfwWindowShouldClose(g_pWindow) == 0);

	// Cleanup VBO and shader
	for (unsigned int i = 0; i < 2; i++) {
		glDeleteBuffers(1, &example[i].vertexbuffer);
		glDeleteBuffers(1, &example[i].uvbuffer);
		glDeleteBuffers(1, &example[i].normalbuffer);
		glDeleteBuffers(1, &example[i].elementbuffer);
		glDeleteTextures(1, &example[i].Texture);
		glDeleteVertexArrays(1, &example[i].VertexArrayID);
	}
	glDeleteProgram(programID);

	// Terminate AntTweakBar and GLFW
	TwTerminate();
	glfwTerminate();

	return 0;
}

