#include "imgui.h"
#include "imgui_impl_sdl_gl3.h"
#include <stdio.h>
#include <GL/glew.h>
#include<SDL.h>
#include <math.h>
#include <string.h>
#include <vector>
#include <algorithm>
#include <stdint.h>

#define sizeof_array(t) (sizeof(t)/sizeof(t[0]))
const float NODE_SLOT_RADIUS = 5.0f;
const ImVec2 NODE_WINDOW_PADDING(8.0f, 8.0f);
#define MAX_CONNECTION_COUNT 32
static inline ImVec2 operator+(const ImVec2& lhs, const ImVec2& rhs) {return ImVec2(lhs.x+rhs.x, lhs.y+rhs.y); }
static inline ImVec2 operator-(const ImVec2& lhs, const ImVec2& rhs) {return ImVec2(lhs.x-rhs.x, lhs.y-rhs.y); }

static void error_callback(int error, const char* description)
{
	fprintf(stderr, "Error %d: %s\n", error, description);
}

static uint32_t s_id = 0;

enum ConnectionType
{
	ConnectionType_Color,
	ConnectionType_Vec3,
	ConnectionType_Float,
	ConnectionType_Int,
};

struct ConnectionDesc
{
	const char* name;
	ConnectionType type;
};

struct NodeType
{
	const char* name;
	ConnectionDesc InputConnections[MAX_CONNECTION_COUNT];
	ConnectionDesc OutputConnections[MAX_CONNECTION_COUNT];
};

struct Connection
{
	ImVec2 pos;
	ConnectionDesc desc;

	inline Connection()
	{
		pos.x = pos.y = 0.0f;
		input = 0;
	}

	union 
	{ 
		float v3[3];
		float v;
		int i;
	};

	struct Connection* input;
	std::vector<Connection*> output;
};

static struct NodeType s_nodeTypes[] =
{
	{
		"Multiply",
		// Input
		{
			{"Input1", ConnectionType_Float},
			{"Input2", ConnectionType_Float},
		},
		{
			{"Out", ConnectionType_Float},
		},
	},

	{
		"Add",
		{
			{"Input1", ConnectionType_Float},
			{"Input2", ConnectionType_Float},
		},
		{
			{"Out", ConnectionType_Float},
		},
	},
};

struct Node 
{
	ImVec2 pos;
	ImVec2 size;
	int id;
	const char* name;
	std::vector<Connection*> inputConnections;
	std::vector<Connection*> outputConnections;
};

static void setupConnections(std::vector<Connection*>& connections, ConnectionDesc* connectionDescs)
{
	for (int i = 0; i < MAX_CONNECTION_COUNT; ++i)
	{
		const ConnectionDesc& desc = connectionDescs[i];

		if(!desc.name)
			break;
		
		Connection* con = new Connection;
		con->desc = desc;

		connections.push_back(con);
	}
}

static Node* createNodeFromType(ImVec2 pos, NodeType* nodeType)
{
	Node* node = new Node;
	node->id = s_id++;
	node->name = nodeType->name;

	ImVec2 titleSize = ImGui::CalcTextSize(node->name);

	titleSize.y *= 3;

	setupConnections(node->inputConnections, nodeType->InputConnections);
	setupConnections(node->outputConnections, nodeType->OutputConnections);

	ImVec2 inputTextSize(0.0f, 0.0f);
	ImVec2 outputText(0.0f, 0.0f);

	for (Connection* c : node->inputConnections)
	{
		ImVec2 textSize = ImGui::CalcTextSize(c->desc.name);
		inputTextSize.x = std::max<float>(textSize.x, inputTextSize.x);

		c->pos = ImVec2(0.0f, titleSize.y + inputTextSize.y + textSize.y / 2.0f);

		inputTextSize.y += textSize.y;
		inputTextSize.y += 4.0f;
	}

	inputTextSize.x += 40.0f;
	// max text size + 40 pixels in between
	
	float xStart = inputTextSize.x;

	for (Connection* c : node->outputConnections)
	{
		ImVec2 textSize = ImGui::CalcTextSize(c->desc.name);
		inputTextSize.x = std::max<float>(xStart + textSize.x, inputTextSize.x);
	}

	node->pos = pos;
	node->size.x = inputTextSize.x;
	node->size.y = inputTextSize.y + titleSize.y;

	inputTextSize.y = 0.0f;

	for (Connection* c : node->outputConnections)
	{
		ImVec2 textSize = ImGui::CalcTextSize(c->desc.name);

		c->pos = ImVec2(node->size.x, titleSize.y + inputTextSize.y + textSize.y / 2.0f);

		inputTextSize.y += textSize.y;
		inputTextSize.y += 4.0f;
	}

	return node;
}

Node* createNodeFromName(ImVec2 pos, const char* name)
{
	for (int i = 0; i < (int)sizeof_array(s_nodeTypes); ++i)
	{
		if(!strcmp(s_nodeTypes[i].name, name))
			return createNodeFromType(pos, &s_nodeTypes[i]);
	}

	return 0;
}

enum DragState
{
	DragState_Default,
	DragState_Hover,
	DragState_BeginDrag,
	DragState_Dragging,
	DragState_Connect,
};

struct DragNode
{
	ImVec2 pos;
	Connection* con;
};

static DragNode s_dragNode;
static DragState s_dragState = DragState_Default;

static std::vector<Node*> s_nodes;

void drawHermite(ImDrawList* drawList, ImVec2 p1, ImVec2 p2, int STEPS)
{
	ImVec2 t1 = ImVec2(+80.0f, 0.0f);
	ImVec2 t2 = ImVec2(+80.0f, 0.0f);

	for (int step = 0; step <= STEPS; step++)
	{
		float t = (float)step / (float) STEPS;
		float h1 = +2*t*t*t - 3*t*t + 1.0f;
		float h2 = -2*t*t*t + 3*t*t;
		float h3 =    t*t*t - 2*t*t + t;
		float h4 =    t*t*t -   t*t;
		drawList->PathLineTo(ImVec2(h1*p1.x + h2*p2.x + h3*t1.x + h4*t2.x, 
									h1*p1.y + h2*p2.y + h3*t1.y + h4*t2.y));
	}

	drawList->PathStroke(ImColor(200,200,100), false, 3.0f);
}

static bool isConnectorHovered(Connection* c, ImVec2 offset)
{
	ImVec2 mousePos = ImGui::GetIO().MousePos;
	ImVec2 conPos = offset + c->pos;

	float xd = mousePos.x - conPos.x;
	float yd = mousePos.y - conPos.y;

	return ((xd * xd) + (yd * yd)) < (NODE_SLOT_RADIUS * NODE_SLOT_RADIUS);
}

static Connection* getHoverCon(ImVec2 offset, ImVec2* pos)
{
	for (Node* node : s_nodes)
	{
		ImVec2 nodePos = node->pos + offset;

		for (Connection* con : node->inputConnections)
		{
			if (isConnectorHovered(con, nodePos))
			{
				*pos = nodePos + con->pos;
				return con;
			}
		}

		for (Connection* con : node->outputConnections)
		{
			if (isConnectorHovered(con, nodePos))
			{
				*pos  = nodePos + con->pos;
				return con;
			}
		}
	}

	s_dragNode.con = 0;
	return 0;
}

void updateDragging(ImVec2 offset)
{
	switch (s_dragState)
	{
		case DragState_Default:
		{
			ImVec2 pos;
			Connection* con = getHoverCon(offset, &pos);

			if (con)
			{
				s_dragNode.con = con;
				s_dragNode.pos = pos;
				s_dragState = DragState_Hover;
				return;
			}

			break;
		}
		case DragState_Hover:
		{
			ImVec2 pos;
			Connection* con = getHoverCon(offset, &pos);

			if (con != s_dragNode.con)
			{
				s_dragNode.con = 0;
				s_dragState = DragState_Default;
				return;
			}

			if(ImGui::IsMouseClicked(0) && s_dragNode.con)
				s_dragState = DragState_Dragging;

			break;
		}

		case DragState_BeginDrag:
		{
			break;
		}

		case DragState_Dragging:
		{
			ImDrawList* drawList = ImGui::GetWindowDrawList();

			drawList->ChannelsSetCurrent(0);

			drawHermite(drawList, s_dragNode.pos, ImGui::GetIO().MousePos, 12);

			if (!ImGui::IsMouseDown(0))
			{
				ImVec2 pos;
				Connection* con = getHoverCon(offset, &pos);

				if (con == s_dragNode.con)
				{
					s_dragNode.con = 0;
					s_dragState = DragState_Default;
					return;
				}

				con->input = s_dragNode.con;
				s_dragNode.con = 0;
				s_dragState = DragState_Default;
			}

			break;
		}

		case DragState_Connect:
		{
			break;
		}
	}
}

static void displayNode(ImDrawList* drawList, ImVec2 offset, Node* node, int& node_selected)
{
	int node_hovered_in_scene = -1;
	bool open_context_menu = false;

	ImGui::PushID(node->id);
	ImVec2 node_rect_min = offset + node->pos;

	drawList->ChannelsSetCurrent(1);
	bool old_any_active = ImGui::IsAnyItemActive();

	ImVec2 textSize = ImGui::CalcTextSize(node->name);

	ImVec2 pos = node_rect_min + NODE_WINDOW_PADDING;
	pos.x = node_rect_min.x + (node->size.x / 2) - textSize.x / 2;

	ImGui::SetCursorScreenPos(pos);
	ImGui::Text("%s", node->name);

	// Save the size of what we have emitted and weither any of the widgets are being used
	bool node_widgets_active = (!old_any_active && ImGui::IsAnyItemActive());
	ImVec2 node_rect_max = node_rect_min + node->size;

	// Display node box
	drawList->ChannelsSetCurrent(0);

	ImGui::SetCursorScreenPos(node_rect_min);
	ImGui::InvisibleButton("node", node->size);

	if (ImGui::IsItemHovered())
	{
		node_hovered_in_scene = node->id;
		open_context_menu |=ImGui::IsMouseClicked(1);
	}

	bool node_moving_active = false;

	if(ImGui::IsItemActive() && !s_dragNode.con)
		node_moving_active = true;

	ImU32 node_bg_color = node_hovered_in_scene == node->id ? ImColor(75,75,75) : ImColor(60,60,60);
	drawList->AddRectFilled(node_rect_min, node_rect_max, node_bg_color, 4.0f);

	ImVec2 titleArea = node_rect_max;
	titleArea.y = node_rect_min.y + 30.0f;

	drawList->AddRectFilled(node_rect_min + ImVec2(1,1), titleArea, ImColor(100,0,0), 4.0f);
	drawList->AddRect(node_rect_min, node_rect_max, ImColor(100,100,100), 4.0f);

	ImVec2 off;

	offset.y += 40.0f;

	offset = offset + node_rect_min;

	off.x = node_rect_min.x;
	off.y = node_rect_min.y;

	for (Connection* con : node->inputConnections)
	{
		ImGui::SetCursorScreenPos(offset + ImVec2(10.0f, 0));
		ImGui::Text("%s", con->desc.name);

		ImColor conColor = ImColor(150,150,150);

		if(isConnectorHovered(con, node_rect_min))
			conColor = ImColor(200,200,200);

		drawList->AddCircleFilled(node_rect_min + con->pos, NODE_SLOT_RADIUS, conColor);

		offset.y += textSize.y + 2.0f;
	}

	offset = node_rect_min;
	offset.y += 40.0f;

	for (Connection* con : node->outputConnections)
	{
		textSize = ImGui::CalcTextSize(con->desc.name);
		ImGui::SetCursorScreenPos(offset + ImVec2(con->pos.x - (textSize.x + 10.0f), 0));
		ImGui::Text("%s", con->desc.name);

		ImColor conColor = ImColor(150,150,150);

		if(isConnectorHovered(con, node_rect_min))
			conColor = ImColor(200,200,200);

		drawList->AddCircleFilled(node_rect_min + con->pos, NODE_SLOT_RADIUS, conColor);

		offset.y += textSize.y + 2.0f;
	}

	if (node_widgets_active || node_moving_active)
		node_selected = node->id;

	if(node_moving_active && ImGui::IsMouseDragging(0))
		node->pos = node->pos + ImGui::GetIO().MouseDelta;

	ImGui::PopID();
}

Node* findNodeByCon(Connection* findCon)
{
	for (Node* node : s_nodes)
	{
		for (Connection* con : node->inputConnections)
		{
			if(con == findCon)
				return node;
		}

		for (Connection* con : node->outputConnections)
		{
			if(con == findCon)
				return node;
		}
	}

	return 0;
}

void renderLines(ImDrawList* drawList, ImVec2 offset)
{
	for (Node* node : s_nodes)
	{
		for (Connection* con : node->inputConnections)
		{
			if(!con->input)
				continue;

			Node* targetNode = findNodeByCon(con->input);

			if(!targetNode)
				continue;

			drawHermite(drawList,
				offset + targetNode->pos + con->input->pos,
				offset + node->pos + con->pos,
				12);
		}
	}
}

static void ShowExampleAppCustomNodeGraph(bool* opened)
{
	ImGui::SetNextWindowSize(ImVec2(700,600), ImGuiSetCond_FirstUseEver);
	if (!ImGui::Begin("Example: Custom Node Graph", opened))
	{
		ImGui::End();
		return;
	}

	bool open_context_menu = false;
	int node_hovered_in_list = -1;
	int node_hovered_in_scene = -1;

	static int node_selected = -1;
	static ImVec2 scrolling = ImVec2(0.0f, 0.0f);

#if 0
	static ImVector<Node> nodes;
	static ImVector<NodeLink> links;
	static bool inited = false;
	static ImVec2 scrolling = ImVec2(0.0f, 0.0f);
	static int node_selected = -1;
	if (!inited)
	{
		nodes.push_back(Node(0, "MainTex", ImVec2(40,50), 0.5f, 1, 1));
		nodes.push_back(Node(1, "BumpMap", ImVec2(40,150), 0.42f, 1, 1));
		nodes.push_back(Node(2,"Combine", ImVec2(270,80), 1.0f, 2, 2));
		links.push_back(NodeLink(0, 0, 2, 0));
		links.push_back(NodeLink(1, 0, 2, 1));
		inited = true;
	}

	ImGui::BeginChild("node_list", ImVec2(100,0));
	ImGui::Text("Nodes");
	ImGui::Separator();

	ImGui::BeginChild("node_list", ImVec2(100,0));
	ImGui::Text("Nodes");
	ImGui::Separator();
	for (int node_idx = 0; node_idx < nodes.Size; node_idx++)
	{
		Node* node = &nodes[node_idx];
		ImGui::Text("Nodes");
		ImGui::Separator();
		for (int node_idx = 0; node_idx < nodes.Size; node_idx++)
		{
			Node* node = &nodes[node_idx];
			ImGui::PushID(node->ID);
			if (ImGuiLLIsItemHovered())
			{
				node_hovered_in_list = node->Id;
				open_context_menu |= ImGui::IsMouseClicked(1);
			}
		}
		ImGui::PopID();
	}
	ImGui::EndChild();
#endif

	ImGui::SameLine();
	ImGui::BeginGroup();

	//Create our Child Canvas
	//ImGui::Text("Hold middle mouse button to scroll (%.2f,%.2f)", scrolling.x, scrolling.y);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1,1));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
	ImGui::PushStyleColor(ImGuiCol_ChildWindowBg, ImColor(40,40,40,200));
	ImGui::BeginChild("scrolling_region", ImVec2(0,0), true, ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoMove);
	ImGui::PushItemWidth(120.0f);

	ImDrawList* draw_list = ImGui::GetWindowDrawList();
	draw_list->ChannelsSplit(2);
	// ImVec2 offset = ImGui::GetCursorScreenPos() - scrolling;

		// displayNode(draw_list, scrolling, s_emittable, node_selected);
		// displayNode(draw_list, scrolling, s_emitter, node_selected)

	for(Node* node : s_nodes)
		displayNode(draw_list, scrolling, node, node_selected);

	updateDragging(scrolling);
	renderLines(draw_list, scrolling);

	draw_list->ChannelsMerge();

	if (!ImGui::IsAnyItemHovered() && ImGui::IsMouseHoveringWindow() && ImGui::IsMouseClicked(1))
	{
		node_selected = node_hovered_in_list = node_hovered_in_scene = -1;
		open_context_menu = true;
	}

	if (open_context_menu)
	{
		ImGui::OpenPopup("context_menu");
		if(node_hovered_in_list != -1)
			node_selected = node_hovered_in_list;
		if(node_hovered_in_scene != -1)
			node_selected = node_hovered_in_scene;
	}

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8,8));
	if (ImGui::BeginPopup("context_menu"))
	{
		if (ImGui::MenuItem("Load graph..."))
		{
			/*
			char path[1024];
			if (Dialog_open(path))
			{
			printf("file to load %s\n", path);
			}
			*/
		}

		if (ImGui::MenuItem("Save graph..."))
		{
			/*
			char path[1024];
			if (Dialog_save(path))
			{
			saveNodes(path);
			}
			*/
		}


		/*
		Node* node = node_selected != -1 ? &nodes[node_selected] : NULL;
		ImVec2 scene_pos = ImGui::GetMousePosOnOpeningCurrentPopup() - offset;
		if (node)
		{
		ImGui::Text("Node '%s'", node->Name);
		ImGui::Separator();
		if (ImGui::MenuItem("Rename..", NULL, false, false)) {}
		if (ImGui::MenuItem("Delete", NULL, false, false)) {}
		if (ImGui::MenuItem("Copy", NULL, false, false)) {}
		}
		*/
		//else

		for (int i = 0; i < (int)sizeof_array(s_nodeTypes); ++i)
		{
			if (ImGui::MenuItem(s_nodeTypes[i].name))
			{
				Node* node = createNodeFromType(ImGui::GetIO().MousePos, &s_nodeTypes[i]);
				s_nodes.push_back(node);
			}
		}
		ImGui::EndPopup();
	}
	ImGui::PopStyleVar();

	if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemActive() && ImGui::IsMouseDragging(2, 0.0f))
		scrolling = scrolling - ImGui::GetIO().MouseDelta;

	ImGui::PopItemWidth();
	ImGui::EndChild();
	ImGui::PopStyleColor();
	ImGui::PopStyleVar(2);
	ImGui::EndGroup();

	ImGui::End();
}

int main(int argc, char* argv[])
{
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
	{
		printf("Error: %s\n", SDL_GetError());
		return -1;
	}

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

	SDL_DisplayMode current;
	SDL_GetCurrentDisplayMode(0, &current);
	SDL_Window* window = SDL_CreateWindow("Imgui SDL Opengl3 Example", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);
	SDL_GLContext glContext = SDL_GL_CreateContext(window);

	glewExperimental = GL_TRUE;
	glewInit();

	ImGui_ImplSdlGL3_Init(window);

	bool show_test_window = true;
	bool showExampleTestNodeWindow = true;
	ImVec4 clear_color = ImColor(114, 114, 154);

	bool done = false;

	while (!done)
	{
		SDL_Event Event;
		while (SDL_PollEvent(&Event))
		{
			ImGui_ImplSdlGL3_ProcessEvent(&Event);
			if(Event.type == SDL_QUIT)
				done = true;
		}
		ImGui_ImplSdlGL3_NewFrame(window);

		ShowExampleAppCustomNodeGraph(&showExampleTestNodeWindow);

		glViewport(0, 0, (int)ImGui::GetIO().DisplaySize.x, (int)ImGui::GetIO().DisplaySize.y);
		glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui::Render();
		SDL_GL_SwapWindow(window);
	}

	ImGui_ImplSdlGL3_Shutdown();
	SDL_GL_DeleteContext(glContext);
	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}
/*
	SDL_Init(SDL_INIT_EVERYTHING);

	SDL_Delay(1000);

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

	SDL_Window* window = SDL_CreateWindow("OpenGL", 100, 100, 800, 600, SDL_WINDOW_OPENGL);

	SDL_Event windowEvent;
	while (true)
	{
		if (SDL_PollEvent(&windowEvent))
		{
			if(windowEvent.type == SDL_QUIT)
				break;
		}

		SDL_GL_SwapWindow(window);
	}

	return 0;
	
	{
	static float f = 0.0f;
	ImGui::Text("Hello World!");
	ImGui::SliderFloat("float", &f, 0.0f, 1.0f);
	ImGui::ColorEdit3("clear color", (float*)&clear_color);
	if(ImGui::Button("Test Window")) show_test_window ^= 1;
	if(ImGui::Button("Another Window")) show_another_window ^=1;
	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
	}

	if (show_another_window)
	{
	ImGui::SetNextWindowSize(ImVec2(200,100), ImGuiSetCond_FirstUseEver);
	ImGui::Begin("Another Window", &show_another_window);
	ImGui::Text("Hello");
	ImGui::End();
	}

	if (show_test_window)
	{
	ImGui::SetNextWindowPos(ImVec2(650, 20), ImGuiSetCond_FirstUseEver);
	ImGui::ShowTestWindow();
	}
	
	*/