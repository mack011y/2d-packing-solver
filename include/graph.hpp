#pragma once
#include <vector>
#include <stdexcept>
#include <iostream>

// --- Базовая архитектура Графа ---

// Узел графа
// T - тип данных, хранящихся в узле (например, координаты или информация о фигуре)
template <typename T>
class Node {
private:
    int id;                      // Уникальный идентификатор узла (0, 1, 2...)
    std::vector<int> neighbors;  // Список смежности (хранит ID соседей для каждого "порта")
    T data;                      // Полезная нагрузка узла

public:
    Node() : id(-1), neighbors() {}

    // Конструктор узла
    // max_ports: максимальная степень вершины (например, 4 для квадратной сетки)
    Node(int id, int max_ports, const T& initial_data = T()) 
        : id(id), data(initial_data) {
        neighbors.resize(max_ports, -1); // -1 означает отсутствие связи (пустой порт)
    }

    int get_id() const { return id; }
    
    T& get_data() { return data; }
    const T& get_data() const { return data; }
    void set_data(const T& d) { data = d; }

    // Установить соседа на определенный порт (направление)
    void set_neighbor(int port, int neighbor_id) {
        if (port >= 0 && port < (int)neighbors.size()) {
            neighbors[port] = neighbor_id;
        }
    }

    // Получить ID соседа на определенном порту
    // Возвращает -1, если соседа нет или порт некорректен
    int get_neighbor(int port) const {
        if (port >= 0 && port < (int)neighbors.size()) {
            return neighbors[port];
        }
        return -1;
    }

    const std::vector<int>& get_all_neighbors() const { return neighbors; }
};

// Базовый класс Графа
// T - тип данных в узлах
template <typename T>
class Graph {
protected:
    int max_ports;              // Максимальное количество портов у каждого узла
    std::vector<Node<T>> nodes; // Хранилище всех узлов графа

public:
    explicit Graph(int mp) : max_ports(mp) {}
    virtual ~Graph() = default;

    int get_max_ports() const { return max_ports; }
    size_t size() const { return nodes.size(); }

    // Добавление нового узла в граф
    // Возвращает ID созданного узла
    int add_node(const T& initial_data = T()) {
        int id = (int)nodes.size();
        nodes.emplace_back(id, max_ports, initial_data);
        return id;
    }

    // Добавление направленного ребра от u к v через порт port_u
    void add_directed_edge(int u_id, int v_id, int port_u) {
        if (u_id >= 0 && u_id < (int)nodes.size()) {
            nodes[u_id].set_neighbor(port_u, v_id);
        }
    }

    // Добавление двунаправленного ребра
    // Связывает узел u (через порт port_u) с узлом v (через порт port_v)
    void add_edge(int u_id, int v_id, int port_u, int port_v) {
        add_directed_edge(u_id, v_id, port_u);
        add_directed_edge(v_id, u_id, port_v);
    }

    Node<T>& get_node(int id) {
        if (id < 0 || id >= (int)nodes.size()) throw std::out_of_range("Node ID out of range");
        return nodes[id];
    }
    
    const Node<T>& get_node(int id) const {
        if (id < 0 || id >= (int)nodes.size()) throw std::out_of_range("Node ID out of range");
        return nodes[id];
    }

    const std::vector<Node<T>>& get_nodes() const { return nodes; }
    std::vector<Node<T>>& get_nodes() { return nodes; }
};
