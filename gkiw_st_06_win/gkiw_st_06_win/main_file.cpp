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

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stdlib.h>
#include <stdio.h>
#include "myCube.h"
#include "constants.h"
#include "allmodels.h"
#include "lodepng.h"
#include "shaderprogram.h"
#include <iostream>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

GLuint texs[100];
std::vector<glm::vec4> verts[100];
std::vector<glm::vec4> norms[100];
std::vector<glm::vec2> texCoords[100];
std::vector<unsigned int> indices[100];

//camera stuff
glm::vec3 cameraPos = glm::vec3(0.0f, 0.2f, 0.8f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

float yaw = -90.0f; // yaw is initialized to -90.0 degrees since a yaw of 0.0 results in a direction vector pointing to the right
float pitch = 0.0f;

float lastX = 1024 / 2.0; // window width / 2.0
float lastY = 720 / 2.0;  // window height / 2.0
bool firstMouse = true;

float deltaTime = 0.0f; // Time between current frame and last frame
float lastFrame = 0.0f;  // Time of last frame


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
	if (action == GLFW_PRESS || action == GLFW_REPEAT) {

		float cameraSpeed = 2.5f * deltaTime; // adjust accordingly
		if (key == GLFW_KEY_W)
			cameraPos += cameraSpeed * cameraFront;
		if (key == GLFW_KEY_S)
			cameraPos -= cameraSpeed * cameraFront;
		if (key == GLFW_KEY_A)
			cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
		if (key == GLFW_KEY_D)
			cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
	}
}

void loadModel(int model, std::string plik) {
	using namespace std;
	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(plik,
		aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenSmoothNormals);
	cout << importer.GetErrorString() << endl;

	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
		cerr << "ERROR::ASSIMP:: " << importer.GetErrorString() << endl;
		return;
	}

	aiMesh* mesh = scene->mMeshes[0];

	/*if (scene->HasMeshes()) {//Czy w pliku są siatki wielokątów
		for (int i = 0; i < scene->mNumMeshes; i++) { //Liczba siatek wielokątów
			importujMesh(scene->mMeshes[i]); //Dostęp do konkretnej siatki
		}
	}*/

	for (int i = 0; i < mesh->mNumVertices; i++) {
		aiVector3D vertex = mesh->mVertices[i]; //aiVector3D podobny do glm::vec3

		verts[model].push_back(glm::vec4(vertex.x, vertex.y, vertex.z, 1));

		aiVector3D normal = mesh->mNormals[i]; //Wektory znormalizowane

		norms[model].push_back(glm::vec4(normal.x, normal.y, normal.z, 0));

		/*
		//liczba zdefiniowanych zestawów wsp. teksturowania (zestawów jest max 8)
		unsigned int liczba_zest = mesh->GetNumUVChannels();
		//Liczba składowych wsp. teksturowania dla 0 zestawu.
		unsigned int wymiar_wsp_tex = mesh->mNumUVComponents[0];
		//0 to numer zestawu współrzędnych teksturowania
		aiVector3D texCoord = mesh->mTextureCoords[0][i];
		//x,y,z wykorzystywane jako u,v,w. 0 jeżeli tekstura ma mniej wymiarów
		cout << texCoord.x << " " << texCoord.y << endl;
		*/

		//0 to numer zestawu współrzędnych teksturowania
		// Sprawdzenie, czy współrzędne teksturowania są dostępne
		if (mesh->mTextureCoords[0]) {
			aiVector3D texCoord = mesh->mTextureCoords[0][i];
			texCoords[model].push_back(glm::vec2(texCoord.x, texCoord.y));
		}
		else {
			// Dodanie domyślnych wartości w przypadku braku współrzędnych teksturowania
			texCoords[model].push_back(glm::vec2(0.0f, 0.0f));
		}
	}

	//dla każdego wielokąta składowego
	for (int i = 0; i < mesh->mNumFaces; i++) {
		aiFace& face = mesh->mFaces[i]; //face to jeden z wielokątów siatki
		//dla każdego indeksu->wierzchołka tworzącego wielokąt
		//dla aiProcess_Triangulate to zawsze będzie 3
		for (int j = 0; j < face.mNumIndices; j++) {
			indices[model].push_back(face.mIndices[j]);
		}
		//cout <<endl
	}
	 aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
	 //aiTextureType max18
	 /*
	 for (int i = 0; i < 19; i++) {
		 cout << i << " " << material->GetTextureCount((aiTextureType)i) << endl;
	 }*/
	 //material->

	 //for (int i = 0; i < material->GetTextureCount(aiTextureType_DIFFUSE); i++) {
		// aiString str;//nazwa pliku
		// material->GetTexture(aiTextureType_DIFFUSE, i,	&str);
		// cout << str.C_Str() << endl;
	 //}
}


GLuint readTexture(const char* filename) {
	GLuint tex;
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

	return tex;
}

//Procedura inicjująca
void initOpenGLProgram(GLFWwindow* window) {
	initShaders();
	//************Tutaj umieszczaj kod, który należy wykonać raz, na początku programu************
	glClearColor(0, 0, 0, 1); //Ustaw kolor czyszczenia bufora kolorów
	glEnable(GL_DEPTH_TEST); //Włącz test głębokości na pikselach
	glfwSetKeyCallback(window, key_callback);

	glfwSetCursorPosCallback(window, mouse_callback);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); // Capture the mouse

	//loadModel(std::string("hill.obj"));
	//loadModel(std::string("walls1.fbx"));
	//loadModel(std::string("CastleWall.fbx"));
	//loadModel(std::string("Castle.fbx"));
	//loadModel(std::string("WonderWalls_SecondAge.fbx")); //odwrocone
	//loadModel(std::string("cos1.fbx"));
	//loadModel(std::string("Modularcastle.fbx"));  //jakis dziwny
	//loadModel(std::string("walls.fbx")); //jakis maly fragment
	//loadModel(std::string("astle.fbx")); //jest jakis error
	//loadModel(std::string("castle1.fbx")); //nie dziala
	//loadModel(std::string("Barracks.fbx")); // dziala ale maly
	//loadModel(std::string("castel_wall.fbx")); // nie dziala 
	//loadModel(std::string("scene.fbx")); // nire dziala 
	//loadModel(std::string("spears.fbx")); // dziala
	//loadModel(std::string("stonetower.fbx")); // dziala
	//loadModel(std::string("stonewall.fbx"));

	texs[0] = readTexture("white.png");
	loadModel(0, std::string("OBJ/Free_Mug.obj"));

	texs[1] = readTexture("OBJ/TX_Table_1_1_Base_color.png");
	loadModel(1, std::string("OBJ/Table_Chair_1.obj"));
	

}


//Zwolnienie zasobów zajętych przez program
void freeOpenGLProgram(GLFWwindow* window) {
	freeShaders();
	//************Tutaj umieszczaj kod, który należy wykonać po zakończeniu pętli głównej************
	
	for (int i = 0; i < 100; i++) {
		glDeleteTextures(1, &texs[i]);
	}
}


void drawModel(
	int model,
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

	glUniformMatrix4fv(spLambertTextured->u("M"), 1, false, glm::value_ptr(M)); //Załaduj do programu cieniującego macierz modelu


	glEnableVertexAttribArray(spLambertTextured->a("vertex"));
	glVertexAttribPointer(spLambertTextured->a("vertex"), 4, GL_FLOAT, false, 0, verts[model].data()); //Współrzędne wierzchołków bierz z tablicy myCubeVertices

	glEnableVertexAttribArray(spLambertTextured->a("texCoord"));
	glVertexAttribPointer(spLambertTextured->a("texCoord"), 2, GL_FLOAT, false, 0, texCoords[model].data()); //Współrzędne teksturowania bierz z tablicy myCubeTexCoords

	glEnableVertexAttribArray(spLambertTextured->a("normal"));
	glVertexAttribPointer(spLambertTextured->a("normal"), 4, GL_FLOAT, false, 0, norms[model].data()); //Współrzędne wierzchołków bierz z tablicy myCubeVertices

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texs[model]);
	glUniform1i(spLambertTextured->u("tex"), 0);

	//glDrawArrays(GL_TRIANGLES, 0, myCubeVertexCount);
	glDrawElements(GL_TRIANGLES,indices[model].size(),GL_UNSIGNED_INT, indices[model].data());

	glDisableVertexAttribArray(spLambertTextured->a("vertex"));
	glDisableVertexAttribArray(spLambertTextured->a("color"));
	glDisableVertexAttribArray(spLambertTextured->a("normal"));
}

//Procedura rysująca zawartość sceny
void drawScene(GLFWwindow* window) {
	//************Tutaj umieszczaj kod rysujący obraz******************l
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); //Wyczyść bufor koloru i bufor głębokości

	glm::mat4 M = glm::mat4(1.0f); //Zainicjuj macierz modelu macierzą jednostkową
	glm::mat4 V = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp); // Compute view matrix based on camera position and direction
	glm::mat4 P = glm::perspective(glm::radians(50.0f), 1024.0f / 720.0f, 0.1f, 100.0f); // Adjusted near and far planes

	spLambertTextured->use(); //Aktywuj program cieniujący

	glUniformMatrix4fv(spLambertTextured->u("P"), 1, false, glm::value_ptr(P)); //Załaduj do programu cieniującego macierz rzutowania
	glUniformMatrix4fv(spLambertTextured->u("V"), 1, false, glm::value_ptr(V)); //Załaduj do programu cieniującego macierz widoku
	//*********************************************************************

	drawModel(0, P, V, M, glm::vec3(0.0f, 0.075f, 0.0f), 10.0f, glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.011f, 0.011f, 0.011f));
	drawModel(1, P, V, M, glm::vec3(0.0f, 0.0f, 0.0f), 10.0f, glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.001f, 0.001f, 0.001f));
	glfwSwapBuffers(window); //Skopiuj bufor tylny do bufora przedniego
}


int main(void)
{
	GLFWwindow* window; //Wskaźnik na obiekt reprezentujący okno

	glfwSetErrorCallback(error_callback);//Zarejestruj procedurę obsługi błędów

	if (!glfwInit()) { //Zainicjuj bibliotekę GLFW
		fprintf(stderr, "Nie można zainicjować GLFW.\n");
		exit(EXIT_FAILURE);
	}

	window = glfwCreateWindow(1024, 720, "OpenGL", NULL, NULL);  //Utwórz okno 500x500 o tytule "OpenGL" i kontekst OpenGL.

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
	float angle_x = 0; //zadeklaruj zmienną przechowującą aktualny kąt obrotu
	float angle_y = 0; //zadeklaruj zmienną przechowującą aktualny kąt obrotu
	glfwSetTime(0); //Wyzeruj licznik czasu
	while (!glfwWindowShouldClose(window)) //Tak długo jak okno nie powinno zostać zamknięte
	{
		float currentFrame = glfwGetTime();
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;
		
		drawScene(window); //Wykonaj procedurę rysującą
		glfwPollEvents(); //Wykonaj procedury callback w zalezności od zdarzeń jakie zaszły.
	}

	freeOpenGLProgram(window);

	glfwDestroyWindow(window); //Usuń kontekst OpenGL i okno
	glfwTerminate(); //Zwolnij zasoby zajęte przez GLFW
	exit(EXIT_SUCCESS);
}
