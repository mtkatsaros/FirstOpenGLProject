#include "Object3D.h"
#include "ShaderProgram.h"
#include <glm/ext.hpp>

glm::mat4 Object3D::buildModelMatrix() const {
	auto m = glm::translate(glm::mat4(1), m_position);
	m = glm::translate(m, m_center * m_scale);
	m = glm::rotate(m, m_orientation[2], glm::vec3(0, 0, 1));
	m = glm::rotate(m, m_orientation[0], glm::vec3(1, 0, 0));
	m = glm::rotate(m, m_orientation[1], glm::vec3(0, 1, 0));
	m = glm::scale(m, m_scale);
	m = glm::translate(m, -m_center);
	m = m * m_baseTransform;
	return m;
}

Object3D::Object3D(std::vector<Mesh3D>&& meshes)
	: Object3D(std::move(meshes), glm::mat4(1)) {
}

Object3D::Object3D(std::vector<Mesh3D>&& meshes, const glm::mat4& baseTransform)
	: m_meshes(meshes), m_position(), m_orientation(), m_scale(1.0),
	m_center(), m_baseTransform(baseTransform), m_material(0.1, 1.0, 0.3, 4), m_velocity(), 
	m_acceleration(), m_rot_velocity(), m_rot_acceleration(), m_mass(1.0), m_forces()
{
	//add gravity because it is a universal constant
	m_forces.push_back(GRAVITATIONAL_ACCELERATION * m_mass);
}

const glm::vec3& Object3D::getPosition() const {
	return m_position;
}

const glm::vec3& Object3D::getOrientation() const {
	return m_orientation;
}

const glm::vec3& Object3D::getScale() const {
	return m_scale;
}

/**
 * @brief Gets the center of the object's rotation.
 */
const glm::vec3& Object3D::getCenter() const {
	return m_center;
}

const glm::vec3& Object3D::getVelocity() const {
	return m_velocity;
}
const glm::vec3& Object3D::getAcceleration() const {
	return m_acceleration;
}
const glm::vec3& Object3D::getRotationalVelocity() const {
	return m_rot_velocity;
}
const glm::vec3& Object3D::getRotationalAcceleration() const {
	return m_rot_acceleration;
}
const float& Object3D::getMass() const {
	return m_mass;
}
const std::vector<glm::vec3>& Object3D::getForces() const {
	return m_forces;
}


const std::string& Object3D::getName() const {
	return m_name;
}

const glm::vec4& Object3D::getMaterial() const {
	return m_material;
}

size_t Object3D::numberOfChildren() const {
	return m_children.size();
}

const Object3D& Object3D::getChild(size_t index) const {
	return m_children[index];
}

Object3D& Object3D::getChild(size_t index) {
	return m_children[index];
}


void Object3D::setPosition(const glm::vec3& position) {
	m_position = position;
}

void Object3D::setOrientation(const glm::vec3& orientation) {
	m_orientation = orientation;
}

void Object3D::setScale(const glm::vec3& scale) {
	m_scale = scale;
}

/**
 * @brief Sets the center point of the object's rotation, which is otherwise a rotation around
   the origin in local space..
 */
void Object3D::setCenter(const glm::vec3& center)
{
	m_center = center;
}

void Object3D::setName(const std::string& name) {
	m_name = name;
}

void Object3D::setMaterial(const glm::vec4& material) {
	m_material = material;
}

void Object3D::setVelocity(const glm::vec3& velocity) {
	m_velocity = velocity;
}

void Object3D::setAcceleration(const glm::vec3& acceleration) {
	m_acceleration = acceleration;
}

void Object3D::setRotationalVelocity(const glm::vec3& rotVelocity) {
	m_rot_velocity = rotVelocity;
}

void Object3D::setRotationalAcceleration(const glm::vec3& rotAcceleration) {
	m_rot_acceleration = rotAcceleration;
}

void Object3D::setMass(const float& mass) {
	m_mass = mass;
	//since we changed the mass, we must update the acceleration due to gravity
	clearForces();
}

void Object3D::addForce(const glm::vec3& force) {
	m_forces.push_back(force);
}

// I forgot that c++ removal of elements in vectors needs iterators,
// so I just decided to make a function that clears all forces to make it 
// simpler. Here is the Geeks for Geeks link I double checked my work with even though 
// I didn't really need it lol: https://www.geeksforgeeks.org/vector-erase-and-clear-in-cpp/
void Object3D::clearForces() {
	m_forces.clear();
	//add gravity back because it is a constant force
	m_forces.push_back(GRAVITATIONAL_ACCELERATION * m_mass);
}

void Object3D::tick(float dt) {
	if (m_position.y == 0) {
		//Add mu : frictional contant of a surface; negative gravity so no need to flip sign
		float muX = std::abs(GRAVITATIONAL_ACCELERATION.y * MU);
		float muZ = std::abs(GRAVITATIONAL_ACCELERATION.y * MU);
		glm::vec3 direction = glm::normalize(glm::vec3(m_velocity.x, 0, m_velocity.z));
		float xSign = (m_velocity.x < 0.0) ? 1.0 : -1.0;
		float zSign = (m_velocity.z < 0.0) ? 1.0 : -1.0;
	

		addForce(glm::vec3(std::abs(direction.x) * xSign * muX * m_mass, 0, std::abs(direction.z) * zSign * muZ * m_mass));

		//if the object has stopped horizontally, we no longer want to apply friction
		if (m_velocity.x == 0)
			clearForces();

		// finally, apply the normal force against gravity
		addForce(-GRAVITATIONAL_ACCELERATION * m_mass);
	}
	// add up all the forces to get the new acceleration (boy do I miss kinematic physics)
	// update the frictional forces 	
	glm::vec3 netForce(0, 0, 0);
	for (glm::vec3& f : m_forces)
		netForce += f;

	//yes; in Michael physics mass can equal 0. So we must account for that.
	if(m_mass > 0)
		m_acceleration = netForce / m_mass;

	//now clear the forces
	clearForces();
	//we will make it so that the y coordinate of an object never dips below 0
	if (m_position.y < 0 && m_mass != 0)
		m_position = glm::vec3(m_position.x, 0, m_position.z);

	// rebuild the matrix with updated position
	m_velocity += m_acceleration * dt;
	m_position += m_velocity * dt;
	//prevent studdering. This could be fixed in the future with collisions.
	if (m_position.y < 0)
		m_position.y = 0;
	buildModelMatrix();
		
	//apply to any children
	for (auto& c : m_children) {
		c.tick(dt);
	}
}

void Object3D::move(const glm::vec3& offset) {
	m_position = m_position + offset;
}

void Object3D::rotate(const glm::vec3& rotation) {
	m_orientation = m_orientation + rotation;
}

void Object3D::grow(const glm::vec3& growth) {
	m_scale = m_scale * growth;
}

void Object3D::addChild(Object3D&& child) {
	m_children.emplace_back(child);
}

void Object3D::render(ShaderProgram& shaderProgram) const {
	renderRecursive(shaderProgram, glm::mat4(1));
}

/**
 * @brief Renders the object and its children, recursively.
 * @param parentMatrix the model matrix of this object's parent in the model hierarchy.
 */
void Object3D::renderRecursive(ShaderProgram& shaderProgram, const glm::mat4& parentMatrix) const {
	// This object's true model matrix is the combination of its parent's matrix and the object's matrix.
	glm::mat4 trueModel = parentMatrix * buildModelMatrix();
	shaderProgram.setUniform("model", trueModel);
	// Render each mesh in the object.
	for (auto& mesh : m_meshes) {
		mesh.render(shaderProgram);
	}
	// Render the children of the object.
	for (auto& child : m_children) {
		child.renderRecursive(shaderProgram, trueModel);
	}
}
