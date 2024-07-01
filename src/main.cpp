/**
This application displays a mesh in wireframe using "Modern" OpenGL 3.0+.
The Mesh3D class now initializes a "vertex array" on the GPU to store the vertices
	and faces of the mesh. To render, the Mesh3D object simply triggers the GPU to draw
	the stored mesh data.
We now transform local space vertices to clip space using uniform matrices in the vertex shader.
	See "simple_perspective.vert" for a vertex shader that uses uniform model, view, and projection
		matrices to transform to clip space.
	See "uniform_color.frag" for a fragment shader that sets a pixel to a uniform parameter.
*/
#define _USE_MATH_DEFINES
#include <glad/glad.h>
#include <iostream>
#include <memory>
#include <filesystem>
#include <math.h>

#include "AssimpImport.h"
#include "Mesh3D.h"
#include "Object3D.h"
#include "Animator.h"
#include "ShaderProgram.h"
#include <SFML/Window/Event.hpp>
#include <SFML/Window/Window.hpp>

// Constant to set max point lights
const int MAX_POINT_LIGHTS = 10;

// Constant to set max spotlights
const int MAX_SPOTLIGHTS = 10;

struct Scene {
	ShaderProgram program;
	std::vector<Object3D> objects;
	std::vector<Animator> animators;
};

/**
 * @brief Constructs a shader program that applies the Phong reflection model.
 */
ShaderProgram phongLightingShader() {
	ShaderProgram shader;
	try {
		// These shaders are INCOMPLETE.
		shader.load("shaders/light_perspective.vert", "shaders/lighting.frag");
	}
	catch (std::runtime_error& e) {
		std::cout << "ERROR: " << e.what() << std::endl;
		exit(1);
	}
	return shader;
}


/**
* @brief Initializes Phong lighting shader
*/
void phongInit(ShaderProgram &program, float shininess) {
	program.activate();
	program.setUniform("material.shininess", shininess);
	program.setUniform("numPointLights", 0); // Modify when point lights are added
}


/**
* @brief Adds directional lighting to the scene using Phong lighting shader
*/
void addDirectionalLight(ShaderProgram &program, glm::vec3 direction, glm::vec3 ambient, 
	glm::vec3 diffuse, glm::vec3 specular) {
	program.setUniform("dirLight.direction", direction);
	program.setUniform("dirLight.ambient", ambient);
	program.setUniform("dirLight.diffuse", diffuse);
	program.setUniform("dirLight.specular", specular);
}


/**
* @brief Adds a point light to the scene using Phong lighting shader and attenuation
*/
void addPointLight(ShaderProgram& program, glm::vec3 position, float constant,
float linear, float quadratic, glm::vec3 ambient, glm::vec3 diffuse,
glm::vec3 specular, int pointLightIndex) {	
	//TODO: set some sort of warning when out of bounds.
	program.setUniform("numPointLights", pointLightIndex + 1);
	program.setUniform("pointLights[" + std::to_string(pointLightIndex) + "].position", position);
	program.setUniform("pointLights[" + std::to_string(pointLightIndex) + "].constant", constant);
	program.setUniform("pointLights[" + std::to_string(pointLightIndex) + "].linear", linear);
	program.setUniform("pointLights[" + std::to_string(pointLightIndex) + "].quadratic", quadratic);
	program.setUniform("pointLights[" + std::to_string(pointLightIndex) + "].ambient", ambient);
	program.setUniform("pointLights[" + std::to_string(pointLightIndex) + "].diffuse", diffuse);
	program.setUniform("pointLights[" + std::to_string(pointLightIndex) + "].specular", specular);
}




/**
 * @brief Constructs a shader program that performs texture mapping with no lighting.
 */
ShaderProgram texturingShader() {
	ShaderProgram shader;
	try {
		shader.load("shaders/texture_perspective.vert", "shaders/texturing.frag");
	}
	catch (std::runtime_error& e) {
		std::cout << "ERROR: " << e.what() << std::endl;
		exit(1);
	}
	return shader;
}

/**
 * @brief Loads an image from the given path into an OpenGL texture.
 */
Texture loadTexture(const std::filesystem::path& path, const std::string& samplerName = "baseTexture") {
	StbImage i;
	i.loadFromFile(path.string());
	return Texture::loadImage(i, samplerName);
}

/*****************************************************************************************
*  DEMONSTRATION SCENES
*****************************************************************************************/
Scene bunny() {
	Scene scene{ phongLightingShader() };

	// We assume that (0,0) in texture space is the upper left corner, but some artists use (0,0) in the lower
	// left corner. In that case, we have to flip the V-coordinate of each UV texture location. The last parameter
	// to assimpLoad controls this. If you load a model and it looks very strange, try changing the last parameter.
	auto bunny = assimpLoad("models/bunny_textured.obj", true);
	bunny.grow(glm::vec3(9, 9, 9));
	bunny.move(glm::vec3(0.2, -1, 0));

	//Initialize light values
	phongInit(scene.program, 32.0);

	// Add directional light (high noon)
	glm::vec3 direction = glm::vec3(0, -1, -1);
	glm::vec3 ambient = glm::vec3(0.1, 0.1, 0.085);
	glm::vec3 diffuse = glm::vec3(0, 0, 0);
	glm::vec3 specular = glm::vec3(1, 1, .85);
	addDirectionalLight(scene.program, direction, ambient, diffuse, specular);

	// Add a point light
	glm::vec3 position = glm::vec3(0, 2, 2);
	float constant = 1.0;
	float linear = 0.09;
	float quadratic = 0.032;
	glm::vec3 ambientPoint = glm::vec3(0, 0, 0);
	glm::vec3 diffusePoint = glm::vec3(0, 0, 0);
	glm::vec3 specularPoint = glm::vec3(0, 0, 0);
	addPointLight(scene.program, position, constant, linear, quadratic, ambientPoint, diffusePoint, specularPoint, 0);


	// Move all objects into the scene's objects list.
	scene.objects.push_back(std::move(bunny));
	// Now the "bunny" variable is empty; if we want to refer to the bunny object, we need to reference 
	// scene.objects[0]

	Animator spinBunny;
	// Spin the bunny 360 degrees over 10 seconds.
	spinBunny.addAnimation(std::make_unique<RotationAnimation>(scene.objects[0], 10.0, glm::vec3(0, 2 * M_PI, 0)));
	
	// Move all animators into the scene's animators list.
	scene.animators.push_back(std::move(spinBunny));

	return scene;
}


/**
 * @brief Demonstrates loading a square, oriented as the "floor", with a manually-specified texture
 * that does not come from Assimp.
 */
Scene marbleSquare() {
	Scene scene{ phongLightingShader() };

	std::vector<Texture> textures = {
		loadTexture("models/White_marble_03/Textures_2K/white_marble_03_2k_baseColor.tga", "baseTexture"),
	};

	//Initialize light values
	phongInit(scene.program, 32.0);

	// Add directional light (none)
	glm::vec3 direction = glm::vec3(0, -1, 0);
	glm::vec3 ambientDir = glm::vec3(0, 0, 0);
	glm::vec3 diffuseDir = glm::vec3(0, 0, 0);
	glm::vec3 specularDir = glm::vec3(0, 0, 0);
	addDirectionalLight(scene.program, direction, ambientDir, diffuseDir, specularDir);

	// Add a point light
	glm::vec3 position = glm::vec3(0, 0, 0);
	float constant = 1.0;
	float linear = 0.09;
	float quadratic = 0.032;
	glm::vec3 ambientPoint = glm::vec3(0, 0, .2);
	glm::vec3 diffusePoint = glm::vec3(.8, .8, .6);
	glm::vec3 specularPoint = glm::vec3(1, 1, .75);
	addPointLight(scene.program, position, constant, linear, quadratic, ambientPoint, diffusePoint, specularPoint, 0);


	auto mesh = Mesh3D::square(textures);
	auto floor = Object3D(std::vector<Mesh3D>{mesh});
	floor.grow(glm::vec3(5, 5, 5));
	floor.move(glm::vec3(0, -1.5, 0));
	floor.rotate(glm::vec3(-M_PI / 2, 0, 0));

	scene.objects.push_back(std::move(floor));
	return scene;
}

/**
 * @brief Loads a cube with a cube map texture.
 */
Scene cube() {
	Scene scene{ texturingShader() };

	auto cube = assimpLoad("models/cube.obj", true);

	scene.objects.push_back(std::move(cube));

	Animator spinCube;
	spinCube.addAnimation(std::make_unique<RotationAnimation>(scene.objects[0], 10.0, glm::vec3(0, 2 * M_PI, 0)));
	// Then spin around the x axis.
	spinCube.addAnimation(std::make_unique<RotationAnimation>(scene.objects[0], 10.0, glm::vec3(2 * M_PI, 0, 0)));

	scene.animators.push_back(std::move(spinCube));

	return scene;
}

/**
 * @brief Constructs a scene of a tiger sitting in a boat, where the tiger is the child object
 * of the boat.
 * @return
 */
Scene lifeOfPi() {
	// This scene is more complicated; it has child objects, as well as animators.
	Scene scene{ phongLightingShader() };

	auto boat = assimpLoad("models/boat/boat.fbx", true);
	boat.move(glm::vec3(0, -0.7, 0));
	boat.grow(glm::vec3(0.01, 0.01, 0.01));
	auto tiger = assimpLoad("models/tiger/scene.gltf", true);
	tiger.move(glm::vec3(0, -5, 10));
	// Move the tiger to be a child of the boat.
	boat.addChild(std::move(tiger));


	//Initialize light values
	phongInit(scene.program, 32.0);

	// Add directional light (midnight)
	glm::vec3 direction = glm::vec3(10, -1, 0);
	glm::vec3 ambientDir = glm::vec3(0.05, 0.05, 0.05);
	glm::vec3 diffuseDir = glm::vec3(0, 0, 0);
	glm::vec3 specularDir = glm::vec3(0.05, 0.05, 0.05);
	addDirectionalLight(scene.program, direction, ambientDir, diffuseDir, specularDir);

	// Add a point light
	glm::vec3 position = glm::vec3(0, 20, 0);
	float constant = 1.0;
	float linear = 0.09;
	float quadratic = 0.032;
	glm::vec3 ambientPoint = glm::vec3(0, 0, 0);
	glm::vec3 diffusePoint = glm::vec3(.8, .8, .6);
	glm::vec3 specularPoint = glm::vec3(1, 1, .75);
	addPointLight(scene.program, position, constant, linear, quadratic, ambientPoint, diffusePoint, specularPoint, 0);

	// Move the boat into the scene list.
	scene.objects.push_back(std::move(boat));

	// We want these animations to referenced the *moved* objects, which are no longer
	// in the variables named "tiger" and "boat". "boat" is now in the "objects" list at
	// index 0, and "tiger" is the index-1 child of the boat.
	Animator animBoat;
	animBoat.addAnimation(std::make_unique<RotationAnimation>(scene.objects[0], 10, glm::vec3(0, 2 * M_PI, 0)));
	Animator animTiger;
	animTiger.addAnimation(std::make_unique<RotationAnimation>(scene.objects[0].getChild(1), 10, glm::vec3(0, 0, 2 * M_PI)));

	// The Animators will be destroyed when leaving this function, so we move them into
	// a list to be returned.
	scene.animators.push_back(std::move(animBoat));
	scene.animators.push_back(std::move(animTiger));

	// Transfer ownership of the objects and animators back to the main.
	return scene;
}



int main() {
	
	std::cout << std::filesystem::current_path() << std::endl;
	
	// Initialize the window and OpenGL.
	sf::ContextSettings settings;
	settings.depthBits = 24; // Request a 24 bits depth buffer
	settings.stencilBits = 8;  // Request a 8 bits stencil buffer
	settings.antialiasingLevel = 2;  // Request 2 levels of antialiasing
	settings.majorVersion = 3;
	settings.minorVersion = 3;
	sf::Window window(sf::VideoMode{ 1200, 800 }, "Modern OpenGL", sf::Style::Resize | sf::Style::Close, settings);

	gladLoadGL();
	glEnable(GL_DEPTH_TEST);

	// Inintialize scene objects.
	auto myScene = marbleSquare();
	// You can directly access specific objects in the scene using references.
	auto& firstObject = myScene.objects[0];

	// Activate the shader program.
	myScene.program.activate();

	// Set up the view and projection matrices.
	glm::vec3 cameraPos = glm::vec3(0, 0, 5);
	glm::mat4 camera = glm::lookAt(cameraPos, glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
	glm::mat4 perspective = glm::perspective(glm::radians(45.0), static_cast<double>(window.getSize().x) / window.getSize().y, 0.1, 100.0);
	myScene.program.setUniform("view", camera);
	myScene.program.setUniform("projection", perspective);
	myScene.program.setUniform("cameraPos", cameraPos);

	// Ready, set, go!
	bool running = true;
	sf::Clock c;
	auto last = c.getElapsedTime();

	// Start the animators.
	for (auto& anim : myScene.animators) {
		anim.start();
	}

	while (running) {
		
		sf::Event ev;
		while (window.pollEvent(ev)) {
			if (ev.type == sf::Event::Closed) {
				running = false;
			}
		}
		auto now = c.getElapsedTime();
		auto diff = now - last;
		std::cout << 1 / diff.asSeconds() << " FPS " << std::endl;
		last = now;

		// Update the scene.
		for (auto& anim : myScene.animators) {
			anim.tick(diff.asSeconds());
		}

		// Clear the OpenGL "context".
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		// Render the scene objects.
		for (auto& o : myScene.objects) {
			o.render(myScene.program);
		}
		window.display();


		
	}

	return 0;
}


