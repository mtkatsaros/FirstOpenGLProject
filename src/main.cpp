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
#include "TranslationAnimation.h"

// Constant to set max point lights
const int MAX_POINT_LIGHTS = 20;
int currentPointLights;

// Constant to set max spotlights
const int MAX_SPOTLIGHTS = 10;
int currentSpotLights;

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
	program.setUniform("numSpotLights", 0); // Modify when spotlights are added
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
* @brief Sets directional lighting to daytime
*/
void setToDayTime(ShaderProgram& program) {
	// clear color sets background
	// source: https://registry.khronos.org/OpenGL-Refpages/gl4/html/glClearColor.xhtml
	glClearColor(0.68, 0.85, 0.9, 1);
	glm::vec3 direction = glm::vec3(0, -1, 0);
	glm::vec3 ambientDir = glm::vec3(.3, .3, .255);
	glm::vec3 diffuseDir = glm::vec3(1, 1, .85);
	glm::vec3 specularDir = glm::vec3(.3, .3, .255);
	addDirectionalLight(program, direction, ambientDir, diffuseDir, specularDir);
}

/**
* @brief Sets directional lighting to night lighting
*/
void setToNightTime(ShaderProgram& program) {
	glClearColor(0, 0, 0, 1);
	glm::vec3 direction = glm::vec3(0, -1, 0);
	glm::vec3 ambientDir = glm::vec3(.01, .01, .01);
	glm::vec3 diffuseDir = glm::vec3(0, 0, 0);
	glm::vec3 specularDir = glm::vec3(.03, .03, .03);
	addDirectionalLight(program, direction, ambientDir, diffuseDir, specularDir);
}


/**
* @brief Adds a point light to the scene using Phong lighting shader and attenuation
*/
void addPointLight(ShaderProgram& program, glm::vec3 position, float constant,
float linear, float quadratic, glm::vec3 ambient, glm::vec3 diffuse,
glm::vec3 specular, int pointLightIndex) {	
	//DONE: set some sort of warning when out of bounds.
	if (pointLightIndex >= MAX_POINT_LIGHTS || pointLightIndex < 0) {
		std::cerr << "Point light index out of bounds. You may see unusual results in your lighting." << std::endl;
		return;
	}

	//modify the amount of spot lights on the scene if the new index hits the current array size
	if(pointLightIndex >= currentPointLights)
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
* @brief Adds a spotlight to the scene using Phong lighting shader, attenuation, and spotlight intensity
*/
void addSpotLight(ShaderProgram& program, glm::vec3 position, glm::vec3 direction, float cutOff, float outerCutOff, float constant,
	float linear, float quadratic, glm::vec3 ambient, glm::vec3 diffuse,
	glm::vec3 specular, int spotLightIndex) {
	if (spotLightIndex >= MAX_SPOTLIGHTS || spotLightIndex < 0) {
		std::cerr << "Spotlight index out of bounds. You may see unusual results in your lighting." << std::endl;
		return;
	}

	//modify the amount of spotlights on the scene if the new index hits the current array size
	if (spotLightIndex >= currentPointLights)
		program.setUniform("numSpotLights", spotLightIndex + 1);
	program.setUniform("spotLights[" + std::to_string(spotLightIndex) + "].position", position);
	program.setUniform("spotLights[" + std::to_string(spotLightIndex) + "].direction", direction);
	program.setUniform("spotLights[" + std::to_string(spotLightIndex) + "].cutOff", cutOff);
	program.setUniform("spotLights[" + std::to_string(spotLightIndex) + "].outerCutOff", outerCutOff);
	program.setUniform("spotLights[" + std::to_string(spotLightIndex) + "].constant", constant);
	program.setUniform("spotLights[" + std::to_string(spotLightIndex) + "].linear", linear);
	program.setUniform("spotLights[" + std::to_string(spotLightIndex) + "].quadratic", quadratic);
	program.setUniform("spotLights[" + std::to_string(spotLightIndex) + "].ambient", ambient);
	program.setUniform("spotLights[" + std::to_string(spotLightIndex) + "].diffuse", diffuse);
	program.setUniform("spotLights[" + std::to_string(spotLightIndex) + "].specular", specular);
}

void moveFlashLight(ShaderProgram& program, glm::vec3 position, glm::vec3 direction) {
	//position -= glm::vec3(0, 2, 0);
	program.setUniform("spotLights[0].position", position);
	program.setUniform("spotLights[0].direction", direction);
}

void toggleFlashLight(ShaderProgram& program, bool toggledOn) {
	if (toggledOn) {
		program.setUniform("spotLights[0].ambient", glm::vec3(1, 1, 1));
		program.setUniform("spotLights[0].diffuse", glm::vec3(0.8, 0.8, 0.8));
		program.setUniform("spotLights[0].specular", glm::vec3(1, 1, 1));
		return;
	}
	program.setUniform("spotLights[0].ambient", glm::vec3(0, 0, 0));
	program.setUniform("spotLights[0].diffuse", glm::vec3(0, 0, 0));
	program.setUniform("spotLights[0].specular", glm::vec3(0, 0, 0));
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
Texture loadTexture(const std::filesystem::path& path, const std::string& samplerName = "material.baseTexture") {
	StbImage i;
	i.loadFromFile(path.string());
	return Texture::loadImage(i, samplerName);
}

const int TOTAL_ROCK_MAX = 1; //needed a global rock maximum since it is accessed in 2 places
const glm::vec3 ROCK_DISPLACEMENT = glm::vec3(1000, 0, 0);
const float ROCK_MASS = 0.5;
/**
 * @brief adds a rock to the scene, throws it, and deletes after 10 seconds
 */
void throwRock(Scene& scene, glm::vec3 position, glm::vec3 direction, int& rockCount) {
	if (rockCount <= 0) {
		std::cout << "Out of rocks!" << std::endl;
		return;
	}
	//create the rock on the scene and move it to the character's position
	auto& rock = scene.objects[rockCount + 2];
	rock.move(position - ROCK_DISPLACEMENT);
	rock.setVelocity(direction * glm::vec3(50,50,50));
	//TODO: Implement throwing physics

	//remove that rock
	rockCount--;
}

/**
 * @brief My main scene. Holds all the logic for the game.
 */
Scene mainScene() {
	Scene scene{ phongLightingShader() };

	// grass for the ground
	std::vector<Texture> textures = {
		loadTexture("models/grass/grass01.jpg", "material.baseTexture"),
		loadTexture("models/grass/grass01_n.jpg", "material.normalMap"),
		loadTexture("models/grass/grass01_s.jpg", "material.specularMap")
	};
	auto mesh = Mesh3D::square(textures);
	auto floor = Object3D(std::vector<Mesh3D>{mesh});
	// physics professors would hate me. Set mass to 0. Don't hate the developer, hate the game.
	floor.setMass(0);
	floor.grow(glm::vec3(5, 5, 5));
	floor.move(glm::vec3(0, 0, 0));
	floor.rotate(glm::vec3(-M_PI / 2, 0, 0));


	//Trees
	const int TREE_COUNT = 20;
	glm::vec3 treePos = glm::vec3(-100, 12.5, -100);
	// Object3D does not have a default constructor, so I cannot initialize the vector size to TREE_COUNT
	// and perform a range based for loop. This is the work-around so that we do not call any default 
	// constructors that don't exist.
	std::vector<Object3D> trees = { assimpLoad("models/tree/scene.gltf", true) };
	trees.back().setMass(0);
	trees.back().grow(glm::vec3(10, 10, 10));
	trees.back().move(treePos);
	

	for (int i = 1; i < TREE_COUNT; i++) {
		trees.emplace_back(assimpLoad("models/tree/scene.gltf", true));
		trees.back().setMass(0);
		trees.back().grow(glm::vec3(10, 10, 10));
		trees.back().move(glm::vec3(treePos.x + 50, treePos.y, treePos.z));
		
		treePos += glm::vec3(20, 0, 0);
		if (treePos.x >= 100) {
			treePos = glm::vec3(-100, treePos.y, treePos.z + 50);
		}
	}

	//rocks
	std::vector<Object3D> rocks = { assimpLoad("models/rock/scene.gltf", true) };
	rocks.back().grow(glm::vec3(0.3, 0.3, 0.3));
	rocks.back().move(ROCK_DISPLACEMENT);
	rocks.back().setMass(ROCK_MASS);
	for (int i = 1; i < TOTAL_ROCK_MAX; i++) {
		rocks.emplace_back(assimpLoad("models/rock/scene.gltf", true));
		rocks.back().grow(glm::vec3(0.3, 0.3, 0.3));
		//the rock origin is (0, -0.7, 0). Preload them below the map to improve framerate.
		rocks.back().move(ROCK_DISPLACEMENT);
		rocks.back().setMass(ROCK_MASS);
	}
	
	//rat
	auto rat = assimpLoad("models/rat/street_rat_4k.gltf", true);
	rat.setMass(0);
	rat.grow(glm::vec3(30, 30, 30));
	rat.move(glm::vec3(0.2, -1.5, 0));
	
	//monster
	auto monster = assimpLoad("models/monster/scene.gltf", true);
	monster.grow(glm::vec3(4.5, 4.5, 4.5));
	monster.move(glm::vec3(13, -1.5, 33));

	//Initialize light values
	phongInit(scene.program, 32.0);

	// set time of day by adding directional light
	setToDayTime(scene.program);
	//setToNightTime(scene.program);

	scene.objects.push_back(std::move(floor)); //pos 0
	scene.objects.push_back(std::move(rat)); //pos 1
	scene.objects.push_back(std::move(monster)); //pos 2
	for (Object3D r : rocks) //start at position 3 (desired index + 2)
		scene.objects.push_back(std::move(r));
	for (Object3D t : trees)
		scene.objects.push_back(std::move(t));

	Animator animRat;
	animRat.addAnimation(std::make_unique<TranslationAnimation>(scene.objects[1], 30, glm::vec3(0, 10, 0)));

	scene.animators.push_back(std::move(animRat));
	return scene;
}

/*****************************************************************************************
*  DEMONSTRATION SCENES
*****************************************************************************************/

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
	sf::Window window(sf::VideoMode{ 2000, 1333 }, "Michael's Scene", sf::Style::Resize | sf::Style::Close, settings);

	gladLoadGL();
	glEnable(GL_DEPTH_TEST);

	// Inintialize scene objects.
	auto myScene = mainScene();
	// You can directly access specific objects in the scene using references.
	auto& firstObject = myScene.objects[0];

	// Activate the shader program.
	myScene.program.activate();

	// Set up the view and projection matrices.
	glm::vec3 cameraPos = glm::vec3(0, 10, 0); //The player is 10 tall. 
	glm::vec3 cameraFront = glm::vec3(0, 0, -1);
	glm::vec3 cameraUp = glm::vec3(0, 1, 0);
	glm::mat4 camera = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
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

	float sensitivity = 0.1;

	//create the character's flashlight. also have a boolean to turn it off
	glm::vec3 flashlightPos = cameraPos;
	glm::vec3 flashlightDir = cameraFront;
	float cutOff = std::cos(glm::radians(12.5));
	float outerCutOff = std::cos(glm::radians(17.5));
	float constant = 1.0;
	float linear = 0.045;
	float quadratic = 0.0075;
	glm::vec3 flashlightAmbient = glm::vec3(1, 1, 1);
	glm::vec3 flashlightDiffuse = glm::vec3(0.8, 0.8, 0.8);
	glm::vec3 flashlightSpecular = glm::vec3(1, 1, 1);
	addSpotLight(myScene.program, flashlightPos, flashlightDir, cutOff, outerCutOff, constant, linear, quadratic, flashlightAmbient, flashlightDiffuse, flashlightSpecular, 0);
	bool flashlightToggled = false;
	toggleFlashLight(myScene.program, flashlightToggled);
	

	// initial x and y postions of the mouse. Window size divided by 2 (center of screen)
	const float X0 = static_cast<float>(window.getSize().x / 2);
	const float Y0 = static_cast<float>(window.getSize().y / 2);

	// initialize yaw and pitch. yaw is -90 to look in the negative z direction
	float yaw = -90.0;
	float pitch = 0.0;

	// use setPosition to center the mouse in the screen
	// sfml documentation for setPosition: https://www.sfml-dev.org/documentation/2.6.1/classsf_1_1Mouse.php
	sf::Mouse::setPosition(sf::Vector2i(window.getSize().x / 2, window.getSize().y / 2), window);
	// hide the mouse cursor
	// sfml documentation for setMouseCursorVisible: https://www.sfml-dev.org/documentation/2.6.1/classsf_1_1Window.php
	window.setMouseCursorVisible(false);
	int rockCount = TOTAL_ROCK_MAX; //initialize the count of rocks
	while (running) {
		
		sf::Event ev;
		while (window.pollEvent(ev)) {
			if (ev.type == sf::Event::Closed) {
				running = false;
			}
			else if (ev.type == sf::Event::KeyPressed) {
				
				switch (ev.key.code) {
				
				case (sf::Keyboard::Key::Escape):
					running = false;
					break;
				
				case(sf::Keyboard::Key::F):
					flashlightToggled = !flashlightToggled;
					toggleFlashLight(myScene.program, flashlightToggled);
					break;
				
				}
			}
			else if (ev.type == sf::Event::MouseButtonPressed) {
				if (ev.mouseButton.button == sf::Mouse::Button::Left)
					throwRock(myScene, cameraPos, glm::normalize(cameraFront), rockCount);
			}
			

			else if (ev.type == sf::Event::MouseMoved) {

				// We need to use Euler angles to implement this. Euler angles are explained in LearnOpenGL camera mechanics.
				// source: https://learnopengl.com/Getting-started/Camera 
				float mousePosX = static_cast<float>(ev.mouseMove.x);
				float mousePosY = static_cast<float>(ev.mouseMove.y);

				// determine the change in x and y positions of the mouse
				float deltaX = mousePosX - X0;
				float deltaY = Y0 - mousePosY; //y coordinates go bottom to top
				
				// implement adjustable sensitivity
				deltaX *= sensitivity;
				deltaY *= sensitivity;

				// Add the change to the yaw and pitch of the camera
				yaw += deltaX;
				pitch += deltaY;

				// we don't want the y axis to pass 90 degrees in either direction. If we do that, it will
				// mess up the math on the other axes.
				if (pitch > 89.0)
					pitch = 89.0;
				if (pitch < -89.0)
					pitch = -89.0;

				// Update the cameraFront to implemnet Euler angles for our direction
				glm::vec3 direction;
				direction.x = std::cos(glm::radians(yaw)) * std::cos(glm::radians(pitch));
				direction.y = sin(glm::radians(pitch));
				direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
				cameraFront = glm::normalize(direction);

				// Now we call glm::lookAt to update the camera
				camera = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
				myScene.program.setUniform("view", camera);

				//move the flashlight with it
				moveFlashLight(myScene.program, cameraPos, cameraFront);
				
				//now reset the mouse position back to the center of the window
				sf::Mouse::setPosition(sf::Vector2i(window.getSize().x / 2, window.getSize().y / 2), window);
			}
		}
		// moved framerate calculation to the top so that I can use it for movement
		auto now = c.getElapsedTime();
		auto diff = now - last;
		std::cout << 1 / diff.asSeconds() << " FPS " << std::endl;
		last = now;
		//calculate speed for movement
		float movementSpeed = 8.5 * diff.asSeconds();
		
		//Tick each object
		for (Object3D& o : myScene.objects) 
			o.tick(diff.asSeconds());
		
			

		// horizontal movement. Since we only want horizontal movement, we need to leave the y coordinate unchanged
		// isKeyPressed works independent from event polling and runs wayyyyyyyyyyyyy smoother
		// info on isKeyPressed: https://www.sfml-dev.org/documentation/2.6.1/classsf_1_1Keyboard.php
		// code logic taken from learnOpenGL camera tutorial I used for the mouse movement logic

		// since openGL's tutorial had x,y,z movement, I wanted only x,y movement. I realized to make it consisitent
		// no matter where the character looks vertically, we have to normalize the vector before doing an operation 
		// on it. (I actually came up with that on my own, my linear algebra brain works again!)
		glm::vec3 frontXZ = glm::normalize(glm::vec3(cameraFront.x, 0, cameraFront.z));
		if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift))
			movementSpeed *= 2.5;
		if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W)) {
			// slow down the movement speed for the case where the movement stacks (I did not derive this mathematically,
			// just trial and error!)
			if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)|| sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D))
				movementSpeed /= 1.5;
			cameraPos += movementSpeed * frontXZ;
			camera = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
			myScene.program.setUniform("view", camera);
			moveFlashLight(myScene.program, cameraPos, cameraFront);
		}
		// had to move the 'S' case above the 'A' case to keep the movement logic consistent
		if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S)) {
			if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D))
				movementSpeed /= 1.5;
			cameraPos -= movementSpeed * frontXZ;
			camera = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
			myScene.program.setUniform("view", camera);
			moveFlashLight(myScene.program, cameraPos, cameraFront);
		}
		//cross product of up and forward gives us position to the right
		if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) {
			cameraPos -= glm::normalize(glm::cross(frontXZ, cameraUp)) * movementSpeed;
			camera = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
			myScene.program.setUniform("view", camera);
			moveFlashLight(myScene.program, cameraPos, cameraFront);
		}
		
		if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) {
			cameraPos += glm::normalize(glm::cross(frontXZ, cameraUp)) * movementSpeed;
			camera = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
			myScene.program.setUniform("view", camera);
			moveFlashLight(myScene.program, cameraPos, cameraFront);
		}
		
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


