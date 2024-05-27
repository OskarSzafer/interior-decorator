/*
Niniejszy program jest wolnym oprogramowaniem; możesz go
rozprowadzać dalej i / lub modyfikować na warunkach Powszechnej
Licencji Publicznej GNU, wydanej przez Fundację Wolnego
Oprogramowania - według wersji 2 tej Licencji lub(według twojego
wyboru) którejś z późniejszych wersji.

Niniejszy program rozpowszechniany jest z nadzieją, iż będzie on
użyteczny - jednak BEZ JAKIEJKOLWIEK GWARANCJI, nawet domyślnej
gwarancji PRZYDATNOŚCI HANDLOWEJ albo PRZYDATNOŚCI DO OKREŚLONYCH
ZASTOSOWAŃ.W celu uzyskania bliższych informacji sięgnij do
Powszechnej Licencji Publicznej GNU.

Z pewnością wraz z niniejszym programem otrzymałeś też egzemplarz
Powszechnej Licencji Publicznej GNU(GNU General Public License);
jeśli nie - napisz do Free Software Foundation, Inc., 59 Temple
Place, Fifth Floor, Boston, MA  02110 - 1301  USA
*/

#define GLM_FORCE_RADIANS
#define GLM_FORCE_SWIZZLE

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stdlib.h>
#include <stdio.h>
#include "constants.h"
#include "lodepng.h"
#include "shaderprogram.h"
#include "myCube.h"
#include "myTeapot.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

struct Furniture
{
	std::string type;
	float position[3];
	float rotation[3];
	float scale;
};
std::vector<Furniture> furnitureList;

std::vector<Furniture> readCSV(const std::string& filename) {
	std::ifstream file(filename);

	if (!file.is_open()) {
		std::cerr << "Failed to open file: " << filename << std::endl;
		return furnitureList;
	}

	std::string line;
	while (std::getline(file, line)) {
		std::istringstream s(line);
		std::string field;
		Furniture furniture;

		// Parse name
		std::getline(s, furniture.type, ',');

		// Parse location
		std::string positonString;
		std::getline(s, positonString, ',');

		std::stringstream ss(positonString);
		std::string item;

		std::getline(ss, item, 'x');
		furniture.position[0] = std::stof(item);

		std::getline(ss, item, 'x');
		furniture.position[1] = std::stof(item);

		std::getline(ss, item, 'x');
		furniture.position[2] = std::stoi(item);

		// Parse rotation
		std::string rotationString;
		std::getline(s, rotationString, ',');

		std::stringstream sb(rotationString);

		std::getline(sb, item, 'x');
		furniture.rotation[0] = std::stof(item);

		std::getline(sb, item, 'x');
		furniture.rotation[1] = std::stof(item);

		std::getline(sb, item, 'x');
		furniture.rotation[2] = std::stoi(item);


		// Parse scale
		std::getline(s, field, ',');
		furniture.scale = std::stof(field);

		furnitureList.push_back(furniture);
	}

	file.close();
	return furnitureList;
}

void printRecords(const std::vector<Furniture>& furnitureList) {
	for (const auto& record : furnitureList) {

		std::cout << "Name: " << record.type
			<< ", Dimensions1: " << record.position[0]
			<< ", Dimensions2: " << record.rotation[2]
			<< ", Value: " << record.scale
			<< std::endl;

}


float aspectRatio=1;

ShaderProgram *sp;

class Model {
	public:
		int elems = 0;
		std::vector<glm::vec4> verts[100];
		std::vector<glm::vec4> norms[100];
		std::vector<glm::vec2> texCoords[100];
		std::vector<unsigned int> indices[100];
		GLuint tex;


		void loadModel(std::string plik) {
			using namespace std;
			Assimp::Importer importer;
			const aiScene* scene = importer.ReadFile(plik,
				aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenSmoothNormals);
			cout << importer.GetErrorString() << endl;

			if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
				cerr << "ERROR::ASSIMP:: " << importer.GetErrorString() << endl;
				return;
			}

			elems = scene->mNumMeshes;

			for (int j = 0; j < scene->mNumMeshes; j++) {
				aiMesh* mesh = scene->mMeshes[j];

				for (int i = 0; i < mesh->mNumVertices; i++) {
					aiVector3D vertex = mesh->mVertices[i]; //aiVector3D podobny do glm::vec3

					verts[j].push_back(glm::vec4(vertex.x, vertex.y, vertex.z, 1));

					aiVector3D normal = mesh->mNormals[i]; //Wektory znormalizowane

					norms[j].push_back(glm::vec4(normal.x, normal.y, normal.z, 0));

					// Sprawdzenie, czy współrzędne teksturowania są dostępne
					if (mesh->mTextureCoords[0]) {
						aiVector3D texCoord = mesh->mTextureCoords[0][i];
						texCoords[j].push_back(glm::vec2(texCoord.x, texCoord.y));
					}
					else {
						// Dodanie domyślnych wartości w przypadku braku współrzędnych teksturowania
						texCoords[j].push_back(glm::vec2(0.0f, 0.0f));
					}
				}

				//dla każdego wielokąta składowego
				for (int i = 0; i < mesh->mNumFaces; i++) {
					aiFace& face = mesh->mFaces[i]; //face to jeden z wielokątów siatki
					//dla każdego indeksu->wierzchołka tworzącego wielokąt
					//dla aiProcess_Triangulate to zawsze będzie 3
					for (int jj = 0; jj < face.mNumIndices; jj++) {
						indices[j].push_back(face.mIndices[jj]);
					}
					//cout <<endl
				}
				aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
			}
		}

		void readTexture(const char* filename) {
			glActiveTexture(GL_TEXTURE0);

			//Wczytanie do pamięci komputera
			std::vector<unsigned char> image;   //Alokuj wektor do wczytania obrazka
			unsigned width, height;   //Zmienne do których wczytamy wymiary obrazka
			//Wczytaj obrazek
			unsigned error = lodepng::decode(image, width, height, filename);

			//Import do pamięci karty graficznej
			glGenTextures(1, &tex); //Zainicjuj jeden uchwyt
			glBindTexture(GL_TEXTURE_2D, tex); //Uaktywnij uchwyt
			//Wczytaj obrazek do pamięci KG skojarzonej z uchwytem
			glTexImage2D(GL_TEXTURE_2D, 0, 4, width, height, 0,
				GL_RGBA, GL_UNSIGNED_BYTE, (unsigned char*)image.data());

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		}


		void drawModel(
			glm::mat4 P,
			glm::mat4 V,
			glm::mat4 MP,
			glm::vec3 translation = glm::vec3(0.0f, 0.0f, 0.0f),
			float angle = 0.0f,
			glm::vec3 rotation = glm::vec3(0.0f, 1.0f, 0.0f),
			glm::vec3 scale = glm::vec3(1.0f, 1.0f, 1.0f)
		) {
			glm::mat4 M = MP;
			M = glm::translate(M, translation);
			M = glm::rotate(M, angle, rotation);
			M = glm::scale(M, scale);
			for (int j = 0; j < elems; j++) {
				glUniformMatrix4fv(sp->u("M"), 1, false, glm::value_ptr(M));
				glUniform4f(sp->u("lp"), 0, 0, -6, 1);

				glEnableVertexAttribArray(sp->a("vertex"));  //Włącz przesyłanie danych do atrybutu vertex
				glVertexAttribPointer(sp->a("vertex"), 4, GL_FLOAT, false, 0, verts[j].data()); //Wskaż tablicę z danymi dla atrybutu vertex


				glEnableVertexAttribArray(sp->a("normal"));  //Włącz przesyłanie danych do atrybutu normal
				glVertexAttribPointer(sp->a("normal"), 4, GL_FLOAT, false, 0, norms[j].data()); //Wskaż tablicę z danymi dla atrybutu normal

				glEnableVertexAttribArray(sp->a("texCoord0"));  //Włącz przesyłanie danych do atrybutu texCoord0
				glVertexAttribPointer(sp->a("texCoord0"), 2, GL_FLOAT, false, 0, texCoords[j].data());


				glUniform1i(sp->u("textureMap0"), 0);
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, tex);

				glDrawElements(GL_TRIANGLES, indices[j].size(), GL_UNSIGNED_INT, indices[j].data());
				glDisableVertexAttribArray(sp->a("vertex"));  //Wyłącz przesyłanie danych do atrybutu vertex
				glDisableVertexAttribArray(sp->a("normal"));
				glDisableVertexAttribArray(sp->a("texCoord0"));//Wyłącz przesyłanie danych do atrybutu normal
			}
		}
};

//camera
glm::vec3 cameraPos = glm::vec3(0.0f, 0.2f, 0.8f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

bool w = false;
bool s = false;
bool a = false;
bool d = false;

float yaw = -90.0f;
float pitch = 0.0f;

float lastX = 1024 / 2.0; // window width / 2.0
float lastY = 720 / 2.0;  // window height / 2.0
bool firstMouse = true;

float deltaTime = 0.0f; // Time between current frame and last frame
float lastFrame = 0.0f;  // Time of last frame

Model model1;
Model model2;
Model model3;
Model model4;
Model model5;


//Procedura obsługi błędów
void error_callback(int error, const char* description) {
	fputs(description, stderr);
}



void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
	if (firstMouse) {
		lastX = xpos;
		lastY = ypos;
		firstMouse = false;
	}

	float xoffset = xpos - lastX;
	float yoffset = lastY - ypos; // reversed since y-coordinates range from bottom to top
	lastX = xpos;
	lastY = ypos;

	float sensitivity = 0.1f; // adjust as needed
	xoffset *= sensitivity;
	yoffset *= sensitivity;

	yaw += xoffset;
	pitch += yoffset;

	// Make sure that when pitch is out of bounds, screen doesn't get flipped
	if (pitch > 89.0f)
		pitch = 89.0f;
	if (pitch < -89.0f)
		pitch = -89.0f;

	glm::vec3 front;
	front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
	front.y = sin(glm::radians(pitch));
	front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
	cameraFront = glm::normalize(front);
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mod) {
	if (action == GLFW_PRESS) {
		if (key==GLFW_KEY_W) w = true;
		if (key==GLFW_KEY_S) s = true;
		if (key==GLFW_KEY_A) a = true;
		if (key==GLFW_KEY_D) d = true;
	}
	if (action==GLFW_RELEASE) {
		if (key==GLFW_KEY_W) w = false;
		if (key==GLFW_KEY_S) s = false;
		if (key==GLFW_KEY_A) a = false;
		if (key==GLFW_KEY_D) d = false;
	}
}


void windowResizeCallback(GLFWwindow* window,int width,int height) {
    if (height==0) return;
    aspectRatio=(float)width/(float)height;
    glViewport(0,0,width,height);
}

//Procedura inicjująca
void initOpenGLProgram(GLFWwindow* window) {
	//************Tutaj umieszczaj kod, który należy wykonać raz, na początku programu************
	glClearColor(0,0,0,1);
	glEnable(GL_DEPTH_TEST);
	glfwSetWindowSizeCallback(window,windowResizeCallback);
	glfwSetKeyCallback(window,key_callback);

	glfwSetCursorPosCallback(window, mouse_callback);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); // Capture the mouse

	sp=new ShaderProgram("v_simplest.glsl",NULL,"f_simplest.glsl");

	//*******************************Load model********************************
	/*texs[0] = readTexture("white.png");
	loadModel(0, std::string("OBJ/Free_Mug.obj"));

	texs[1] = readTexture("OBJ/TX_Table_1_1_Base_color.png");
	loadModel(1, "OBJ/Table_Chair_1.obj");

	texs[2] = readTexture("OBJ/bench_bcolor.png");
	loadModel(2, "OBJ/bench.fbx");*/

	model1.readTexture("white.png");
	model1.loadModel("OBJ/Free_Mug.obj");

	model2.readTexture("OBJ/TX_Table_1_1_Base_color.png");
	model2.loadModel("OBJ/Table_Chair_1.obj");

	model3.readTexture("OBJ/Masa.png");
	model3.loadModel("OBJ/Masa.fbx");
	std::string filename = "config.txt";
	std::vector<Furniture> furnitures = readCSV(filename);

	if (!furnitures.empty()) {
		printRecords(furnitures);
	}
	else {
		std::cerr << "No records found or failed to read file." << std::endl;
	}
}


//Zwolnienie zasobów zajętych przez program
void freeOpenGLProgram(GLFWwindow* window) {
    //************Tutaj umieszczaj kod, który należy wykonać po zakończeniu pętli głównej************

    delete sp;
}

	
//Procedura rysująca zawartość sceny
void drawScene(GLFWwindow* window) {
	//************Tutaj umieszczaj kod rysujący obraz******************
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glm::mat4 V = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
    glm::mat4 P=glm::perspective(glm::radians(50.0f), 1024.0f / 720.0f, 0.1f, 100.0f);
    glm::mat4 M=glm::mat4(1.0f);

    glUniformMatrix4fv(sp->u("P"),1,false,glm::value_ptr(P));
    glUniformMatrix4fv(sp->u("V"),1,false,glm::value_ptr(V));

    sp->use();//Aktywacja programu cieniującego
    //Przeslij parametry programu cieniującego do karty graficznej
	//************Tutaj umieszczaj kod obiekt***************************

	/*drawModel(0, P, V, M, glm::vec3(0.0f, 0.15f, 0.0f), 0.0f, glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.011f, 0.011f, 0.011f));
	drawModel(1, P, V, M, glm::vec3(0.0f, 0.0f, 0.0f), 0.0f, glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.002f, 0.002f, 0.002f));
	drawModel(2, P, V, M, glm::vec3(0.0f, 0.06f, 0.25f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.003f, 0.003f, 0.003f));
	drawModel(2, P, V, M, glm::vec3(0.0f, 0.06f, -0.25f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.003f, 0.003f, 0.003f));*/

	model1.drawModel(P, V, M, glm::vec3(0.0f, 0.15f, 0.0f), 0.0f, glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.011f, 0.011f, 0.011f));
	model2.drawModel(P, V, M, glm::vec3(0.0f, 0.0f, 0.0f), 0.0f, glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.002f, 0.002f, 0.002f));
	model3.drawModel(P, V, M, glm::vec3(0.0f, 0.06f, 0.25f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.3f, 0.3f, 0.3f));
	for (const auto& record : furnitureList) {
		int obj_id = 0;
		if (record.type == "BED") {
			obj_id = 0;
		}
		else if (record.type == "CHAIR") {
			obj_id = 1;
		}

		drawModel(obj_id, P, V, M, glm::vec3(0.0f, 0.075f, 0.0f), 10.0f, glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(record.scale, record.scale, record.scale));
	}
	//*****************************************************************
    glfwSwapBuffers(window); //Przerzuć tylny bufor na przedni
}


int main(void)
{
	GLFWwindow* window; //Wskaźnik na obiekt reprezentujący okno

	glfwSetErrorCallback(error_callback);//Zarejestruj procedurę obsługi błędów

	if (!glfwInit()) { //Zainicjuj bibliotekę GLFW
		fprintf(stderr, "Nie można zainicjować GLFW.\n");
		exit(EXIT_FAILURE);
	}

	window = glfwCreateWindow(1024, 720, "OpenGL", NULL, NULL);

	if (!window) //Jeżeli okna nie udało się utworzyć, to zamknij program
	{
		fprintf(stderr, "Nie można utworzyć okna.\n");
		glfwTerminate();
		exit(EXIT_FAILURE);
	}

	glfwMakeContextCurrent(window); //Od tego momentu kontekst okna staje się aktywny i polecenia OpenGL będą dotyczyć właśnie jego.
	glfwSwapInterval(1); //Czekaj na 1 powrót plamki przed pokazaniem ukrytego bufora

	if (glewInit() != GLEW_OK) { //Zainicjuj bibliotekę GLEW
		fprintf(stderr, "Nie można zainicjować GLEW.\n");
		exit(EXIT_FAILURE);
	}

	initOpenGLProgram(window); //Operacje inicjujące

	//Główna pętla
	while (!glfwWindowShouldClose(window)) //Tak długo jak okno nie powinno zostać zamknięte
	{
		// Movment and rotation
		float currentFrame = glfwGetTime();
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;

		float cameraSpeed = 0.5f * deltaTime; // adjust accordingly
		if (w == true)
			cameraPos += cameraSpeed * cameraFront;
		if (s == true)
			cameraPos -= cameraSpeed * cameraFront;
		if (a == true)
			cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
		if (d == true)
			cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;

		
		// Scene
		drawScene(window); //Wykonaj procedurę rysującą
		glfwPollEvents(); //Wykonaj procedury callback w zalezności od zdarzeń jakie zaszły.
	}

	freeOpenGLProgram(window);

	glfwDestroyWindow(window); //Usuń kontekst OpenGL i okno
	glfwTerminate(); //Zwolnij zasoby zajęte przez GLFW
	exit(EXIT_SUCCESS);
}
