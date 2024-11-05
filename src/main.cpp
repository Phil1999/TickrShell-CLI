#include "CliApp.h"
#include "MockData.h"
#include <iostream>

int main() {
	try {
		// Create instance of CLI app
		StockTracker::CliApp app;

		// Print the welcome message
		app.printWelcomeMessage();


		// Run app
		app.run();

		// Cleanup and stop app on exit
		app.stop();

	}
	catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
	}


	return 0;
}