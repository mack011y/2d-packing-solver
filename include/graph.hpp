#pragma once
#include <vector>
#include <array>
#include <stdexcept>
#include <iostream>

static constexpr int MAX_PORTS_CAPACITY = 6;

// Узел графа
template <typename T>
class Node {
private:
    int id;                                         
    std::array<int, MAX_PORTS_CAPACITY> neighbors;  
    T data;                                         

public:
    Node() : id(-1) {
        neighbors.fill(-1);
    }

    Node(int id, const T& initial_data = T()) 
        : id(id), data(initial_data) {
    
        neighbors.fill(-1); // -1 означает отсутствие связи (пустой порт)
    }

    int get_id() const { return id; }
    
    T& get_data() { return data; }
    const T& get_data() const { return data; }
    void set_data(const T& d) { data = d; }

    // Установить соседа на определенный порт (направление)
    bool set_neighbor(size_t port, int neighbor_id) {
        if (port < MAX_PORTS_CAPACITY) {
            neighbors[port] = neighbor_id;
            return true;
        }
        return false;
    }

    // Получить ID соседа на определенном порту, вернем -1 если соседа нет/некоректный порт
    int get_neighbor(size_t port) const {
        if (port < MAX_PORTS_CAPACITY) {
            return neighbors[port];
        }
        return -1;
    }

    const std::array<int, MAX_PORTS_CAPACITY>& get_all_neighbors() const { 
        return neighbors; 
    }
};

// Базовый класс Графа
template <typename T>
class Graph {
protected:
    size_t max_ports;              // Максимальное количество портов у каждого узла
    std::vector<Node<T>> nodes;    // Хранилище всех узлов графа

public:
    explicit Graph(size_t mp) : max_ports(mp) {}
    virtual ~Graph() = default;

    size_t get_max_ports() const { return max_ports; }
    
    size_t size() const { return nodes.size(); }

    // Добавление нового узла в граф, вернем ID созданного узла
    int add_node(const T& initial_data = T()) {
        int id = static_cast<int>(nodes.size());
        nodes.emplace_back(id, initial_data);
        return id;
    }

    // Добавление направленного ребра от u к v через порт port_u
    void add_directed_edge(int u_id, int v_id, size_t port_u) {
        if (u_id >= 0 && static_cast<size_t>(u_id) < nodes.size()) {
            nodes[u_id].set_neighbor(port_u, v_id);
        }
    }

    // Добавление двунаправленного ребра
    void add_edge(int u_id, int v_id, size_t port_u, size_t port_v) {
        add_directed_edge(u_id, v_id, port_u);
        add_directed_edge(v_id, u_id, port_v);
    }

    Node<T>& get_node(int id) {
        if (id < 0 || static_cast<size_t>(id) >= nodes.size()) throw std::out_of_range("Node ID out of range");
        return nodes[id];
    }
    
    const Node<T>& get_node(int id) const {
        if (id < 0 || static_cast<size_t>(id) >= nodes.size()) throw std::out_of_range("Node ID out of range");
        return nodes[id];
    }

    const std::vector<Node<T>>& get_nodes() const { return nodes; }
    std::vector<Node<T>>& get_nodes() { return nodes; }
};
