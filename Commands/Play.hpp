// play.hpp - header for the "play" command.
// jun 30, 2021
// chris m.
// https://github.com/real_time_chris

#pragma once

#include "../HelperFunctions.hpp"

namespace discord_core_api {

	class play : public base_function {
	  public:
		static std::unordered_map<uint64_t, uint64_t> timeOfLastPlay;

		play() {
			this->commandName	  = "play";
			this->helpDescription = "add a song to the queue and play it if nothing is playing.";
			embed_data newEmbed;
			newEmbed.setDescription("------\nSimply enter /play songname or url.\n------");
			newEmbed.setTitle("__**play usage:**__");
			newEmbed.setTimeStamp(getTimeAndDate());
			newEmbed.setColor("fe_fe_fe");
			this->helpEmbed = newEmbed;
		}

		unique_ptr<base_function> create() {
			return makeUnique<play>();
		}

		void execute(const base_function_arguments& argsNew) {
			try {
				channel_cache_data channel{ argsNew.getChannelData() };
				guild_data guild{ guilds::getCachedGuild({ argsNew.getInteractionData().guildId }) };
				discord_guild discordGuild{ managerAgent, guild };
				bool areWeAllowed{ checkIfAllowedPlayingInChannel(argsNew.getInputEventData(), discordGuild) };
				if (!areWeAllowed) {
					return;
				}
				guild_member_cache_data guildMember{ argsNew.getGuildMemberData() };
				bool doWeHaveControl = checkIfWeHaveControl(argsNew.getInputEventData(), discordGuild, guildMember);
				if (!doWeHaveControl) {
					return;
				}

				input_event_data newEvent = argsNew.getInputEventData();
				uint64_t currentTime	= std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
				uint64_t previousPlayedTime{ 0 };
				if (play::timeOfLastPlay.contains(argsNew.getInteractionData().guildId.operator const uint64_t&())) {
					previousPlayedTime = play::timeOfLastPlay.at(argsNew.getInteractionData().guildId.operator const uint64_t&());
				}

				if (currentTime - previousPlayedTime < 5000) {
					unique_ptr<embed_data> newEmbed{ makeUnique<embed_data>() };
					newEmbed->setAuthor(argsNew.getUserData().userName,  argsNew.getUserData().getUserImageUrl(user_image_types::Avatar));
					newEmbed->setDescription("------\n__**Sorry, but please wait a total of 5 seconds in between plays!**__\n------");
					newEmbed->setTitle("__**Timing Issue:**__");
					newEmbed->setColor("fefefe");
					respond_to_input_event_data dataPackage(argsNew.getInputEventData());
					dataPackage.setResponseType(input_event_response_type::Ephemeral_Interaction_Response);
					dataPackage.addMessageEmbed(*newEmbed);
					auto newerEvent = input_events::respondToInputEventAsync(dataPackage).get();
					input_events::deleteInputEventResponseAsync(newEvent).get();
					input_events::deleteInputEventResponseAsync(newerEvent, 20000);
					return;
				}

				if (argsNew.getCommandArguments().values.size() == 0 && discord_core_client::getSongAPI(guild.id).areWeCurrentlyPlaying()) {
					unique_ptr<embed_data> newEmbed{ makeUnique<embed_data>() };
					newEmbed->setAuthor(argsNew.getUserData().userName,  argsNew.getUserData().getUserImageUrl(user_image_types::Avatar));
					newEmbed->setDescription("------\n__**Sorry, but there's already something playing!**__\n------");
					newEmbed->setTitle("__**Playing Issue:**__");
					newEmbed->setColor("fefefe");
					respond_to_input_event_data dataPackage{ newEvent };
					dataPackage.setResponseType(input_event_response_type::Ephemeral_Interaction_Response);
					dataPackage.addMessageEmbed(*newEmbed);
					auto newerEvent = input_events::respondToInputEventAsync(dataPackage).get();
					input_events::deleteInputEventResponseAsync(newerEvent, 20000);
					return;
				}

				respond_to_input_event_data dataPackage(argsNew.getInputEventData());
				dataPackage.setResponseType(input_event_response_type::Ephemeral_Deferred_Response);
				newEvent = input_events::respondToInputEventAsync(dataPackage).get();

				previousPlayedTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
				play::timeOfLastPlay.insert_or_assign(argsNew.getInteractionData().guildId.operator const uint64_t&(), previousPlayedTime);
				snowflake voiceChannelId{};
				if (guildMember.getVoiceStateData().channelId != 0) {
					voiceChannelId = guildMember.getVoiceStateData().channelId;
				} else {
					unique_ptr<embed_data> newEmbed{ makeUnique<embed_data>() };
					newEmbed->setAuthor(argsNew.getUserData().userName,  argsNew.getUserData().getUserImageUrl(user_image_types::Avatar));
					newEmbed->setDescription("------\n__**Sorry, but you need to be in a correct voice channel to issue those commands!**__\n------");
					newEmbed->setTitle("__**Playing Issue:**__");
					newEmbed->setColor("fefefe");
					respond_to_input_event_data dataPackageNew{ newEvent };
					dataPackageNew.addMessageEmbed(*newEmbed);
					dataPackageNew.setResponseType(input_event_response_type::Ephemeral_Follow_Up_Message);
					auto newerEvent = input_events::respondToInputEventAsync(dataPackageNew).get();
					input_events::deleteInputEventResponseAsync(newerEvent, 20000);
					return;
				}
				voice_connection& voiceConnection = guild.connectToVoice(guildMember.user.id, guildMember.getVoiceStateData().channelId);
				if (!voiceConnection.areWeConnected()) {
					unique_ptr<embed_data> newEmbed{ makeUnique<embed_data>() };
					newEmbed->setAuthor(argsNew.getUserData().userName,  argsNew.getUserData().getUserImageUrl(user_image_types::Avatar));
					newEmbed->setDescription("------\n__**Sorry, but there is no voice connection that is currently held by me!**__\n------");
					newEmbed->setTitle("__**Connection Issue:**__");
					newEmbed->setColor("fefefe");
					respond_to_input_event_data dataPackageNew{ newEvent };
					dataPackageNew.addMessageEmbed(*newEmbed);
					dataPackageNew.setResponseType(input_event_response_type::Ephemeral_Follow_Up_Message);
					auto newerEvent = input_events::respondToInputEventAsync(dataPackageNew).get();
					input_events::deleteInputEventResponseAsync(newerEvent, 20000);
					return;
				}
				if (guildMember.getVoiceStateData().channelId == 0 || guildMember.getVoiceStateData().channelId != voiceConnection.getChannelId()) {
					unique_ptr<embed_data> newEmbed{ makeUnique<embed_data>() };
					newEmbed->setAuthor(argsNew.getUserData().userName,  argsNew.getUserData().getUserImageUrl(user_image_types::Avatar));
					newEmbed->setDescription("------\n__**Sorry, but you need to be in a correct voice channel to issue those commands!**__\n------");
					newEmbed->setTitle("__**Playing Issue:**__");
					newEmbed->setColor("fefefe");
					respond_to_input_event_data dataPackageNew{ newEvent };
					dataPackageNew.addMessageEmbed(*newEmbed);
					dataPackageNew.setResponseType(input_event_response_type::Ephemeral_Follow_Up_Message);
					auto newerEvent = input_events::respondToInputEventAsync(dataPackageNew).get();
					input_events::deleteInputEventResponseAsync(newerEvent, 20000);
					return;
				}
				jsonifier::vector<song> searchResults{};
				if (argsNew.getCommandArguments().values.size() > 0) {
					auto searchResultsNew =
						discord_core_client::getSongAPI(guild.id).searchForSong(argsNew.getCommandArguments().values["songname"].value.operator jsonifier::string(), 1);
					if (searchResultsNew.size() > 0) {
						searchResults.emplace_back(searchResultsNew.at(0));
					}
				}

				if (searchResults.size() <= 0 && argsNew.getCommandArguments().values.size() > 0) {
					unique_ptr<embed_data> newEmbed{ makeUnique<embed_data>() };
					newEmbed->setAuthor(argsNew.getUserData().userName,  argsNew.getUserData().getUserImageUrl(user_image_types::Avatar));
					newEmbed->setDescription("------\n__**No songs could be found as a result of your search!**__\n------");
					newEmbed->setTitle("__**Search Issue:**__");
					newEmbed->setColor("fefefe");
					respond_to_input_event_data dataPackageNew{ newEvent };
					dataPackageNew.addMessageEmbed(*newEmbed);
					dataPackageNew.setResponseType(input_event_response_type::Ephemeral_Follow_Up_Message);
					auto newerEvent = input_events::respondToInputEventAsync(dataPackageNew).get();
					input_events::deleteInputEventResponseAsync(newerEvent, 20000);
					return;
				}

				int32_t currentPageIndex = 0;
				move_through_message_pages_data returnData{};
				if (searchResults.size() > 0) {
					discordGuild.getDataFromDB(managerAgent);
					size_t songSize						= discordGuild.data.playlist.songQueue.size();
					user_cache_data user					= users::getCachedUser({ .userId = guildMember.user.id });
					searchResults.at(0).addedByUserId	= guildMember.user.id;
					discordGuild.data.playlist.songQueue.emplace_back(searchResults[0]);
					unique_ptr<embed_data> newEmbed{ makeUnique<embed_data>() };
					jsonifier::string descriptionString{};
					discordGuild.writeDataToDB(managerAgent);
					descriptionString = "------\n__**Added the following songs to the queue:\n";
					descriptionString += "[" + searchResults.at(0).songTitle + "](" + searchResults.at(0).viewUrl + ")\n" +
						"Position: " + jsonifier::toString(discordGuild.data.playlist.songQueue.size()) + "\n";
					descriptionString += "**__\n------";
					newEmbed->setAuthor(argsNew.getUserData().userName, argsNew.getUserData().getUserImageUrl(user_image_types::Avatar));
					newEmbed->setDescription(descriptionString);
					newEmbed->setTitle("__**Playing new songs:**__");
					newEmbed->setColor("fefefe");
					respond_to_input_event_data dataPackage{ newEvent };
					dataPackage.setResponseType(input_event_response_type::Edit_Interaction_Response);
					dataPackage.addMessageEmbed(*newEmbed);
					auto newerEvent = input_events::respondToInputEventAsync(dataPackage).get();
					returnData.currentPageIndex -= 1;
				}
				auto channelId = argsNew.getInputEventData().getChannelData().id;
				auto theTask   = [=](song_completion_event_data eventData) mutable -> co_routine<void, false> {
					  auto argsNewer = std::move(argsNew);
					  co_await newThreadAwaitable<void, false>();
					  user_cache_data userNew = users::getCachedUser({ eventData.guildMemberId });
					  std::this_thread::sleep_for(150ms);
					  discordGuild.getDataFromDB(managerAgent);
					  if (discordGuild.data.playlist.areThereAnySongs()) {
						  unique_ptr<embed_data> newEmbed{ makeUnique<embed_data>() };
						  if (!eventData.wasItAFail) {
							  discordGuild.getDataFromDB(managerAgent);
							  discordGuild.data.playlist.sendNextSong();
							  discord_core_client::getSongAPI(guild.id).play(discordGuild.data.playlist.currentSong);
							  discordGuild.writeDataToDB(managerAgent);
							  newEmbed->setAuthor(userNew.userName, userNew.getUserImageUrl(user_image_types::Avatar));
							  newEmbed->setDescription("__**Title:**__ [" + discordGuild.data.playlist.currentSong.songTitle + "](" + discordGuild.data.playlist.currentSong.viewUrl +
								  ")" + "\n__**Description:**__ " + discordGuild.data.playlist.currentSong.description + "\n__**Duration:**__ " +
								  discordGuild.data.playlist.currentSong.duration + "\n__**Added By:**__ <@!" + discordGuild.data.playlist.currentSong.addedByUserId + ">");
							  newEmbed->setImage(discordGuild.data.playlist.currentSong.thumbnailUrl);
							  newEmbed->setTitle("__**Now Playing:**__");
							  newEmbed->setColor("fefefe");
							  if (discordGuild.data.playlist.isLoopAllEnabled && discordGuild.data.playlist.isLoopSongEnabled) {
								  newEmbed->setFooter("✅ Loop-All, ✅ Loop-Song");
							  }
							  if (!discordGuild.data.playlist.isLoopAllEnabled && discordGuild.data.playlist.isLoopSongEnabled) {
								  newEmbed->setFooter("❌ Loop-All, ✅ Loop-Song");
							  }
							  if (discordGuild.data.playlist.isLoopAllEnabled && !discordGuild.data.playlist.isLoopSongEnabled) {
								  newEmbed->setFooter("✅ Loop-All, ❌ Loop-Song");
							  }
							  if (!discordGuild.data.playlist.isLoopAllEnabled && !discordGuild.data.playlist.isLoopSongEnabled) {
								  newEmbed->setFooter("❌ Loop-All, ❌ Loop-Song");
							  }
							  create_message_data dataPackage02(channelId);
							  dataPackage02.addMessageEmbed(*newEmbed);
							  messages::createMessageAsync(dataPackage02).get();
						  } else {
							  discordGuild.getDataFromDB(managerAgent);
							  discordGuild.data.playlist.sendNextSong();
							  discordGuild.writeDataToDB(managerAgent);
							  discord_core_client::getSongAPI(guild.id).play(discordGuild.data.playlist.currentSong);
							  newEmbed->setAuthor(userNew.userName, userNew.getUserImageUrl(user_image_types::Avatar));
							  newEmbed->setDescription("__**it appears as though there was an error when trying to play the previous track!**__");
							  newEmbed->setTitle("__**Playing error:**__");
							  newEmbed->setColor("fe0000");
							  if (discordGuild.data.playlist.isLoopAllEnabled && discordGuild.data.playlist.isLoopSongEnabled) {
								  newEmbed->setFooter("✅ Loop-All, ✅ Loop-Song");
							  }
							  if (!discordGuild.data.playlist.isLoopAllEnabled && discordGuild.data.playlist.isLoopSongEnabled) {
								  newEmbed->setFooter("❌ Loop-All, ✅ Loop-Song");
							  }
							  if (discordGuild.data.playlist.isLoopAllEnabled && !discordGuild.data.playlist.isLoopSongEnabled) {
								  newEmbed->setFooter("✅ Loop-All, ❌ Loop-Song");
							  }
							  if (!discordGuild.data.playlist.isLoopAllEnabled && !discordGuild.data.playlist.isLoopSongEnabled) {
								  newEmbed->setFooter("❌ Loop-All, ❌ Loop-Song");
							  }
							  create_message_data dataPackage02(channelId);
							  dataPackage02.addMessageEmbed(*newEmbed);
							  messages::createMessageAsync(dataPackage02).get();
						  }
						  discordGuild.writeDataToDB(managerAgent);
					  } else {
						  unique_ptr<embed_data> newEmbed{ makeUnique<embed_data>() };
						  newEmbed->setAuthor(userNew.userName, userNew.getUserImageUrl(user_image_types::Avatar));
						  newEmbed->setDescription("------\n__**Sorry, but there's nothing left to play here!**__\n------");
						  newEmbed->setTitle("__**Now Playing:**__");
						  newEmbed->setColor("fefefe");
						  if (discordGuild.data.playlist.isLoopAllEnabled && discordGuild.data.playlist.isLoopSongEnabled) {
							  newEmbed->setFooter("✅ Loop-All, ✅ Loop-Song");
						  } else if (!discordGuild.data.playlist.isLoopAllEnabled && discordGuild.data.playlist.isLoopSongEnabled) {
							  newEmbed->setFooter("❌ Loop-All, ✅ Loop-Song");
						  } else if (discordGuild.data.playlist.isLoopAllEnabled && !discordGuild.data.playlist.isLoopSongEnabled) {
							  newEmbed->setFooter("✅ Loop-All, ❌ Loop-Song");
						  } else if (!discordGuild.data.playlist.isLoopAllEnabled && !discordGuild.data.playlist.isLoopSongEnabled) {
							  newEmbed->setFooter("❌ Loop-All, ❌ Loop-Song");
						  }
						  create_message_data dataPackage02(channelId);
						  dataPackage02.addMessageEmbed(*newEmbed);
						  messages::createMessageAsync(dataPackage02).get();
						  discord_core_client::getSongAPI(guild.id).stop();
					  }
					  co_return;
				};
				if (discord_core_client::getSongAPI(guild.id).areWeCurrentlyPlaying()) {
					std::cout << "were currently playing!" << std::endl;
					return;
				} else {
					discord_core_client::getSongAPI(guild.id).onSongCompletion(theTask);
				}
				discordGuild.getDataFromDB(managerAgent);
				if (discordGuild.data.playlist.areThereAnySongs()) {
					discordGuild.getDataFromDB(managerAgent);
					discordGuild.data.playlist.sendNextSong();
					discordGuild.writeDataToDB(managerAgent);
					discord_core_client::getSongAPI(guild.id).play(discordGuild.data.playlist.currentSong);
					unique_ptr<embed_data> newEmbed{ makeUnique<embed_data>() };
					newEmbed->setAuthor(argsNew.getUserData().userName,  argsNew.getUserData().getUserImageUrl(user_image_types::Avatar));
					newEmbed->setDescription("__**Title:**__ [" + discordGuild.data.playlist.currentSong.songTitle + "](" + discordGuild.data.playlist.currentSong.viewUrl + ")" +
						"\n__**Description:**__ " + discordGuild.data.playlist.currentSong.description + "\n__**Duration:**__ " + discordGuild.data.playlist.currentSong.duration +
						"\n__**Added By:**__ <@!" + discordGuild.data.playlist.currentSong.addedByUserId + ">");
					newEmbed->setImage(discordGuild.data.playlist.currentSong.thumbnailUrl);
					newEmbed->setTitle("__**Now Playing:**__");
					newEmbed->setColor("fefefe");
					if (discordGuild.data.playlist.isLoopAllEnabled && discordGuild.data.playlist.isLoopSongEnabled) {
						newEmbed->setFooter("✅ Loop-All, ✅ Loop-Song");
					} else if (!discordGuild.data.playlist.isLoopAllEnabled && discordGuild.data.playlist.isLoopSongEnabled) {
						newEmbed->setFooter("❌ Loop-All, ✅ Loop-Song");
					} else if (discordGuild.data.playlist.isLoopAllEnabled && !discordGuild.data.playlist.isLoopSongEnabled) {
						newEmbed->setFooter("✅ Loop-All, ❌ Loop-Song");
					} else if (!discordGuild.data.playlist.isLoopAllEnabled && !discordGuild.data.playlist.isLoopSongEnabled) {
						newEmbed->setFooter("❌ Loop-All, ❌ Loop-Song");
					}
					respond_to_input_event_data dataPackage02(newEvent);
					dataPackage02.setResponseType(input_event_response_type::Follow_Up_Message);
					dataPackage02.addMessageEmbed(*newEmbed);
					newEvent = input_events::respondToInputEventAsync(dataPackage02).get();

				} else {
					unique_ptr<embed_data> newEmbed{ makeUnique<embed_data>() };
					newEmbed->setAuthor(argsNew.getUserData().userName,  argsNew.getUserData().getUserImageUrl(user_image_types::Avatar));
					newEmbed->setDescription("------\n__**Sorry, but there's nothing to play!**__\n------");
					newEmbed->setTitle("__**Playing Issue:**__");
					newEmbed->setColor("fefefe");
					respond_to_input_event_data dataPackageNew{ newEvent };
					dataPackageNew.setResponseType(input_event_response_type::Ephemeral_Follow_Up_Message);
					dataPackageNew.addMessageEmbed(*newEmbed);
					auto newerEvent = input_events::respondToInputEventAsync(dataPackageNew).get();
					input_events::deleteInputEventResponseAsync(newerEvent, 20000);
				}
			} catch (const std::runtime_error& error) {
				std::cout << "play::execute()" << error.what() << std::endl;
			}
		};
		~play(){};
	};
	std::unordered_map<uint64_t, uint64_t> play::timeOfLastPlay;

};// namespace discord_core_api
