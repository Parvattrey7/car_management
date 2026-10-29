#define ASIO_STANDALONE
#include "crow.h"
#include <sqlite3.h>
#include <iostream>
#include <string>

// Helper function to execute SQL queries
void executeSQL(sqlite3* db, const std::string& sql) {
    char* errMsg = nullptr;
    sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg);
    if (errMsg) {
        std::cerr << "SQL Error: " << errMsg << std::endl;
        sqlite3_free(errMsg);
    }
}

int main() {
    crow::SimpleApp app;
    sqlite3* db;

    // 1. Initialize SQLite Database
    if (sqlite3_open("cars.db", &db)) {
        std::cerr << "Can't open database" << std::endl;
        return 1;
    }

    std::string createTable = R"(
        CREATE TABLE IF NOT EXISTS Cars (
            ID INTEGER PRIMARY KEY AUTOINCREMENT,
            Make TEXT NOT NULL,
            Model TEXT NOT NULL,
            Year INTEGER,
            Price REAL,
            Color TEXT
        );
    )";
    executeSQL(db, createTable);

    // 2. Serve Frontend HTML Pages
    CROW_ROUTE(app, "/")([](){ return crow::mustache::load_text("index.html"); });
    CROW_ROUTE(app, "/add")([](){ return crow::mustache::load_text("add.html"); });
    CROW_ROUTE(app, "/dashboard")([](){ return crow::mustache::load_text("dashboard.html"); });

    // 3. API Route: Get all cars (with search & filter options)
    CROW_ROUTE(app, "/api/cars")([&db](const crow::request& req){
        std::string search_query = req.url_params.get("q") ? req.url_params.get("q") : "";
        std::string sort_by = req.url_params.get("sort") ? req.url_params.get("sort") : "id_desc";
        
        std::string order_sql = " ORDER BY ID DESC";
        if (sort_by == "price_asc") order_sql = " ORDER BY Price ASC";
        else if (sort_by == "price_desc") order_sql = " ORDER BY Price DESC";
        else if (sort_by == "year_desc") order_sql = " ORDER BY Year DESC";

        std::string sql = "SELECT * FROM Cars WHERE Make LIKE '%" + search_query + 
                          "%' OR Model LIKE '%" + search_query + 
                          "%' OR Color LIKE '%" + search_query + "%'" + order_sql + ";";
        
        crow::json::wvalue::list cars;
        sqlite3_stmt* stmt;
        
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                crow::json::wvalue car;
                car["id"] = sqlite3_column_int(stmt, 0);
                car["make"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                car["model"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                car["year"] = sqlite3_column_int(stmt, 3);
                car["price"] = sqlite3_column_double(stmt, 4);
                car["color"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
                cars.push_back(car);
            }
            sqlite3_finalize(stmt);
        }
        
        crow::json::wvalue result;
        result["cars"] = std::move(cars);
        return crow::response(result);
    });

    // 4. API Route: Add a new car
    CROW_ROUTE(app, "/api/cars/add").methods(crow::HTTPMethod::Post)([&db](const crow::request& req){
        auto body = crow::json::load(req.body);
        if (!body) return crow::response(400, "Invalid JSON");

        // Extract and convert to standard types first
        std::string make = body["make"].s();
        std::string model = body["model"].s();
        int year = body["year"].i();
        double price = body["price"].d();
        std::string color = body["color"].s();

        std::string sql = "INSERT INTO Cars (Make, Model, Year, Price, Color) VALUES ('" + 
                          make + "', '" + model + "', " + std::to_string(year) + ", " + 
                          std::to_string(price) + ", '" + color + "');";
        
        executeSQL(db, sql);
        return crow::response(201, "Car Added");
    });

    // 5. API Route: Edit a car
    CROW_ROUTE(app, "/api/cars/edit/<int>").methods(crow::HTTPMethod::Post)([&db](const crow::request& req, int id){
        auto body = crow::json::load(req.body);
        if (!body) return crow::response(400, "Invalid JSON");

        // Extract and convert to standard types first
        std::string make = body["make"].s();
        std::string model = body["model"].s();
        int year = body["year"].i();
        double price = body["price"].d();
        std::string color = body["color"].s();

        std::string sql = "UPDATE Cars SET Make='" + make + 
                          "', Model='" + model + 
                          "', Year=" + std::to_string(year) + 
                          ", Price=" + std::to_string(price) + 
                          ", Color='" + color + "' WHERE ID=" + std::to_string(id) + ";";
        
        executeSQL(db, sql);
        return crow::response(200, "Car Updated");
    });

    // 6. API Route: Delete a car
    CROW_ROUTE(app, "/api/cars/<int>").methods(crow::HTTPMethod::Delete)([&db](int id){
        std::string sql = "DELETE FROM Cars WHERE ID = " + std::to_string(id) + ";";
        executeSQL(db, sql);
        return crow::response(200, "Car Deleted");
    });

    app.port(8080).multithreaded().run();
    sqlite3_close(db);
    return 0;
}