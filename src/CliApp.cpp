#include "CliApp.h"
#include <spdlog/spdlog.h>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <iostream>
#include <nlohmann/json.hpp>


namespace StockTracker {
    CliApp::CliApp()
        : publisher(zmq::socket_type::pub)
        , subscriber(zmq::socket_type::sub)
    {
        publisher.bind("tcp://*:5556");
        subscriber.connect("tcp://localhost:5555");

        // Use the specific subscribe method
        subscriber.setSubscribe("");

        // Or use generic socket options if needed
        subscriber.setTimeout(1000);

        //spdlog::info("CLI connected to DataService.");

        // Request subscription data from backend
        // Small delay to ensure dataservice is ready.
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        Message msg = Message::makeRequestSubscriptions();
        //spdlog::info("Sending RequestSubscriptions message");
        publisher.send(msg); // Send request for subscriptions
    }

	void CliApp::handleCommand(const std::string& cmd) {
		std::istringstream iss(cmd);
		std::string command;
		iss >> command;

		// Get symbol if present
		std::string symbol;
		iss >> symbol; // Empty if no symbol provided

        switch (hash(command.c_str())) {
        case Commands::Subscribe:
            if (!symbol.empty()) {
                if (!isValidSymbolFormat(symbol)) {
                    spdlog::info("Invalid symbol format. Symbols should be 1-5 uppercase letters.\n");
                    return;
                }
                if (confirmAction("subscribe to", symbol)) {
                    subscribe(symbol);
                }
            }
            else {
                spdlog::warn("Usage: subscribe <symbol>");
            }
            break;

        case Commands::Unsubscribe:
            if (!symbol.empty()) {
                unsubscribe(symbol);
            }
            else {
                spdlog::warn("Usage: unsubscribe <symbol>");
            }
            break;

        case Commands::Query:
            if (!symbol.empty()) {
                query(symbol);
            }
            else {
                spdlog::warn("Usage: query <symbol>");
            }
            break;

        case Commands::Graph:
            if (!symbol.empty()) {
                renderFullGraph(symbol);
            }
            else {
                spdlog::warn("Usage: graph <symbol>");
            }
            break;

        case Commands::History:
            if (!symbol.empty()) {
                requestPriceHistory(symbol);
            }
            else {
                spdlog::warn("Usage: history <symbol>");
            }
            break;

        case Commands::List:
            listStocks();
            break;

        case Commands::Help:
            showHelp();
            showSafetyTips();
            break;

        case Commands::Exit:
            stop();
            break;

        case Commands::Clear:
            clearScreen();
            break;

        default:
            spdlog::warn("Unknown command: {}", command);
            std::cout << "Type 'help' for available commands and safety tips.\n";
            break;
        }
	}

    void CliApp::renderStockList() {
        if (stocks.empty()) {
            std::cout << "No stocks subscribed." << std::endl;
            return;
        }

        std::cout << "Subscribed Stocks:" << std::endl;
        for (const auto& [symbol, data] : stocks) {
            std::cout << symbol << ": $" << data.current_price << " ("
                << data.change_percent << "% change)" << std::endl;
        }

    }

    void CliApp::renderFullGraph(const std::string& symbol) {
        auto it = stocks.find(symbol);
        if (it == stocks.end() || it->second.price_history.empty()) {
            std::cout << "No data available for " << symbol << std::endl;
            return;
        }

        const auto& data = it->second;
        const auto& price_history = data.price_history;

        // Calculate min and max prices
        double min_price = *std::min_element(price_history.begin(), price_history.end());
        double max_price = *std::max_element(price_history.begin(), price_history.end());
        double range = max_price - min_price;

        // Set graph height (number of rows)
        int graph_height = 10;

        // Create bins for price levels (each bin corresponds to one row in the graph)
        std::vector<double> levels(graph_height + 1);
        for (int i = 0; i <= graph_height; ++i) {
            levels[i] = min_price + (range * i / graph_height); // Calculate level thresholds
        }

        std::cout << "Stock Price Graph for " << symbol << ":\n";

        // Render each row (each price level)
        for (int i = graph_height; i >= 0; --i) {
            std::cout << std::fixed << std::setw(6) << std::setprecision(2) << levels[i] << " | ";

            // For each price in history, decide whether to place a star or a space
            for (const auto& price : price_history) {
                if (price >= levels[i] && price < (i == graph_height ? max_price + 1 : levels[i + 1])) {
                    std::cout << '*';  // Price falls in this bin
                }
                else {
                    std::cout << ' ';  // Price doesn't fall in this bin
                }
            }
            std::cout << std::endl;
        }

        // Draw the time axis below the graph
        std::cout << "       ";
        for (size_t i = 0; i < price_history.size(); ++i) {
            std::cout << "-";
        }
        std::cout << "\n       Time ->\n";
    }

    // Commands
    // --------------------------------------------
    void CliApp::subscribe(const std::string& symbol) {
        //spdlog::info("Sending subscribe request for {}", symbol);
        Message msg = Message::makeSubscribe(symbol);
        publisher.send(msg);  // Ensure this send operation is correct
        spdlog::info("Subscribing to {}", symbol);
    }

    void CliApp::unsubscribe(const std::string& symbol) {
        publisher.send(Message::makeUnsubscribe(symbol));
        stocks.erase(symbol);
        spdlog::info("Unsubscribed from {}", symbol);
    }

    void CliApp::query(const std::string& symbol) {
        // Send query request to DataService
        Message msg = Message::makeQuery(symbol);
        publisher.send(msg);

        //spdlog::info("Requested current price for {}", symbol);
    }

    void CliApp::requestPriceHistory(const std::string& symbol) {
        //spdlog::info("Requesting price history for {}", symbol);
        Message msg = Message::makeRequestPriceHistory(symbol);
        publisher.send(msg);
    }

    void CliApp::listStocks() {
        if (stocks.empty()) {
            std::cout << "No stocks subscribed.\n";
        }
        else {
            std::cout << "Subscribed stocks:\n";
            for (const auto& [symbol, data] : stocks) {
                std::cout << symbol << ": $" << data.current_price << " ("
                    << data.change_percent << "% change)\n";
            }
        }
    }

    void CliApp::showHelp() {
        std::cout << "Commands:\n"
            << "  subscribe <symbol>   - Subscribe to stock updates\n"
            << "  unsubscribe <symbol> - Unsubscribe from stock\n"
            << "  query <symbol>       - Get current price for a stock\n"
            << "  graph <symbol>       - Show graph view of stock (price history needed) \n"
            << "  history <symbol>     - Show price history of stock (last 5)\n"
            << "  list                 - Show all subscribed stocks\n"
            << "  help                 - Show this help\n"
            << "  clear                - Clears the terminal\n"
            << "  exit                 - Exit application\n";
    }

    void CliApp::printWelcomeMessage() {
        std::cout << "=====================================\n";
        std::cout << "  Welcome to TickrShell   \n";
        std::cout << "=====================================\n";
        std::cout << "This program allows you to track stock prices in real time.\n";
        std::cout << "You can subscribe to stock updates, query the latest prices, or view price history graphs.\n";
        std::cout << "Type 'help' to see the list of available commands.\n";
        std::cout << "-------------------------------------\n";
        std::cout << "Author: Philip Lee\n";
        std::cout << "-------------------------------------\n";
        std::cout << "Note: for now, it is only possible to subscribe to MSFT, AAPL, GOOGL, AMZN and META \n";
    }

    void CliApp::showUsageCosts() {
        std::cout << "\nUsage Costs and Information:\n"
            << "==============================\n"
            << "1. Data updates: Updates for subscribed stocks are provided every 8 seconds.\n"
            << "2. API Rate Limits: Maximum 100 queries per minute\n"
            << "3. Storage: Price history stored locally using SQLite approximately 34 bytes per row\n\n";
    }

    void CliApp::showSafetyTips() {
        std::cout << "\nSafety Tips:\n";
        std::cout << "1. Always verify stock symbols before subscribing\n";
        std::cout << "2. Use 'query' to check prices before subscribing\n";
        std::cout << "3. Review 'history' to understand price volatility\n";
        std::cout << "4. Use 'list' regularly to track your subscriptions\n";
        std::cout << "5. Clear the screen with 'clear' if it gets cluttered\n\n";
    }


    void CliApp::clearScreen() {
        // Using ANSI escape codes to clear the screen
        std::cout << "\033[2J\033[H";  // Clears the screen and moves the cursor to the top-left
        std::cout.flush();              // Ensures that the output is written immediately

        printWelcomeMessage();
    }

    void CliApp::updateStockData(const StockQuote& quote) {
        auto& data = stocks[quote.symbol];
        data.current_price = quote.price;
        data.change_percent = quote.change_percent.value_or(0.0);

        // Keep a price history for the graph
        data.price_history.push_back(quote.price);
        if (data.price_history.size() > StockData::MAX_HISTORY) {
            data.price_history.pop_front();
        }

        // Log the stock update received
        std::cout << "Received stock update: " << quote.symbol
            << " - $" << quote.price
            << " (" << data.change_percent << "% change)\n";
    }

    bool CliApp::confirmAction(const std::string& action, const std::string& symbol) {
        std::cout << "Are you sure you want to " << action << " " << symbol << "? (y/n): ";
        std::string response;
        std::getline(std::cin, response);
        return (response == "y" || response == "Y");
    }

    bool CliApp::isValidSymbolFormat(const std::string& symbol) {
        // Check if symbol is 1-5 uppercase letters
        if (symbol.empty() || symbol.length() > 5) return false;
        return std::all_of(symbol.begin(), symbol.end(), [](char c) { return std::isupper(c); });
    }

    bool CliApp::isStockSubscribed(const std::string& symbol) {
        return stocks.find(symbol) != stocks.end();
    }

    void CliApp::processUpdates() {
        while (running) {
            // Receive a message from the subscriber
            if (auto msg = subscriber.receive(true)) {
                // If we successfully received a message, handle it
                if (msg->type == MessageType::QuoteUpdate && msg->quote) {
                    if (isStockSubscribed(msg->quote->symbol)) {
                        updateStockData(*msg->quote);  // This is for subscribed stocks
                    }
                    else {
                        // This is a one-time query result, display it but don't subscribe
                        std::cout << "Queried stock: " << msg->quote->symbol
                            << " - $" << msg->quote->price
                            << " (" << msg->quote->change_percent.value_or(0.0) << "% change)\n";
                    }
                }
                else if (msg->type == MessageType::PriceHistoryResponse) {
                    const auto& history = msg->priceHistory;
                    if (history) {
                        auto& data = stocks[msg->symbol];
                        data.price_history.clear();
                        for (const auto& quote : *history) {
                            data.price_history.push_back(quote.price);  // Store only the price in the deque
                        }
                        std::cout << "Price history for: " << msg->symbol << "\n";
                        for (const auto& price : data.price_history) {
                            std::cout << "  $" << price << "\n";
                        }
                    }
                }

                // Handle subscription list from the backend
                else if (msg->type == MessageType::SubscriptionsList) {
                    for (const auto& symbol : msg->subscriptions.value()) {
                        if (!isStockSubscribed(symbol)) {
                            stocks[symbol] = StockData{};  // Add to local stocks map
                            std::cout << "Restored subscription to stock: " << symbol << std::endl;

                            query(symbol);
                        }
                    }
                    std::cout << "Number of subscribed stocks in local cache: " << stocks.size() << "\n";
                    std::cout << "\n> ";
                    std::cout.flush();
                }
                else if (msg->type == MessageType::Subscribe) {
                    // Update the CLI's subscribed stock list
                    if (!isStockSubscribed(msg->symbol)) {
                        stocks[msg->symbol] = StockData{};
                        std::cout << "Subscribed to stock: " << msg->symbol << std::endl;
                    }
                }
                else if (msg->type == MessageType::Error && msg->error) {
                    std::cout << "Error: " << *msg->error << std::endl;
                }
                std::cout.flush();
            }

            // Wait a bit before checking again
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    // Minor issue here with incomplete commands. If user is interrupted when an update happens
    // their previous input wont be cleared.
    void CliApp::run() {
        std::thread update_thread(&CliApp::processUpdates, this);

        // Main CLI loop with input buffering
        std::string input;
        while (running) {
            // Clear any pending input
            std::cout << "\n> ";
            std::cout.flush();  // Make sure prompt is displayed

            if (std::getline(std::cin, input)) {
                if (!input.empty()) {
                    handleCommand(input);
                }
            }
        }

        if (update_thread.joinable()) {
            update_thread.join();
        }
    }

    void CliApp::stop() {
        running = false;
    }
}
