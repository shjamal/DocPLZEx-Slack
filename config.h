#ifndef DOCPLZ_CONFIG_H
#define DOCPLZ_CONFIG_H

// Slack configuration
// Replace these with your actual values from https://api.slack.com/apps
//#define SLACK_BOT_TOKEN L"xoxb-"  // Must start with xoxb-
//#define SLACK_CHANNEL_ID ""            // Usually starts with C or G

// Validation macros
#define SLACK_TOKEN_PREFIX L"xoxb-"
#define SLACK_TOKEN_MIN_LENGTH 50

// Helper function declarations
bool ValidateSlackConfig();

#endif // DOCPLZ_CONFIG_H