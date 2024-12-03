#pragma once
#include <StockTracker/Messages.h>
#include <StockTracker/CurrencyService.h>
#include <unordered_map>
#include <deque>
#include <thread>
#include <zmq.hpp>


constexpr uint32_t hash(const char* str) {
	uint32_t hash = 5381;
	while (*str) {
		hash = ((hash << 5) + hash) + *str++;
	}
	return hash;
}

namespace Commands {
	constexpr auto Subscribe = hash("subscribe");
	constexpr auto Unsubscribe = hash("unsubscribe");
	constexpr auto Query = hash("query");
	constexpr auto Graph = hash("graph");
	constexpr auto List = hash("list");
	constexpr auto History = hash("history");
	constexpr auto Help = hash("help");
	constexpr auto Exit = hash("exit");
	constexpr auto Clear = hash("clear");
	constexpr auto SetCurrency = hash("currency");
}


namespace StockTracker {

	class CliApp {
	private:
		MessageSocket publisher;
		MessageSocket subscriber;

		mutable CurrencyService currency_service;
		std::string display_currency{ "USD" }; // Default currency.


		// Stock data
		struct StockData {
			double current_price{ 0.0 };
			double change_percent{ 0.0 };
			std::string currency{ "USD" };
			std::deque<double> price_history;
			static constexpr size_t MAX_HISTORY = 15;
		};

		std::unordered_map<std::string, StockData> stocks;
		std::atomic<bool> running{ true };


		void handleCommand(const std::string & cmd);
		void processUpdates();
		bool isStockSubscribed(const std::string& symbol);

		// UI rendering
		void renderStockList();
		void renderFullGraph(const std::string& symbol);

		// Command handlers
		void subscribe(const std::string& symbol);
		void unsubscribe(const std::string& symbol);
		void query(const std::string& symbol);
		void requestPriceHistory(const std::string& symbol);
		void listStocks();
		void showHelp();
		void clearScreen();
		

		void updateStockData(const StockQuote& quote);

		// Safety methods
		bool confirmAction(const std::string& action, const std::string& symbol);
		bool isValidSymbolFormat(const std::string& symbol);

		void setCurrency(const std::string& currency);
		void displayPriceInCurrency(double price) const;



	public:
		CliApp();
		void printWelcomeMessage();
		void showUsageCosts();
		void showSafetyTips();
		void run();
		void stop();

		CliApp(const CliApp&) = delete;
		CliApp& operator=(const CliApp&) = delete;

	};
}