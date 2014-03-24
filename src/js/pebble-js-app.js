/*
	Rebble Javascript
*/

var username;
var password;
var password;
var subreddits_enabled;
var subreddits;
var last_subreddit;

var default_subreddits = "adviceanimals,all,AskReddit,aww,bestof,books,earthporn,explainlikeimfive,funny,games,IAmA,movies,music,news,pics,science,technology,television,todayilearned,worldnews";

var modhash = "";

var nt_AppMessageQueue = [];
var nt_AppMessageQueueSize = [];
var nt_AppMessageQueueRunning = [];

var transferInProgress = false;
var transferInProgressURL = "";

var threads = 0;
var loadedThreads = {};

var chunkSize = 0;

var SUBREDDIT_QUEUE = "Subreddit";
var SUBREDDITLIST_QUEUE = "SubredditList";
var THREAD_QUEUE = "Thread";
var NET_IMAGE_QUEUE = "NetImage";
var OTHER_QUEUE = "Other";

/*************************************
	App Message Queue System
*************************************/

function nt_InitAppMessageQueue(name)
{
	nt_AppMessageQueue[name] = [];
	nt_AppMessageQueueSize[name] = 0;
	nt_AppMessageQueueRunning[name] = false;
}

function nt_NextAppMessageQueue(name)
{
	//console.log("nt_NextAppMessageQueue[" + name + "]: " + nt_AppMessageQueueSize[name]);

	if(nt_AppMessageQueueSize[name] === 0)
	{
		nt_AppMessageQueueRunning[name] = false;
		//console.log("nt_NextAppMessageQueue Done");
		return;
	}

	nt_AppMessageQueueSize[name]--;

	var messageObject = nt_AppMessageQueue[name].shift();

	var send = function() {
		Pebble.sendAppMessage(messageObject, function() { nt_NextAppMessageQueue(name) }, send);
	};

	send();
}

function nt_BeginAppMessageQueue(name)
{
	if(nt_AppMessageQueueRunning[name])
	{
		return;
	}

	if(nt_AppMessageQueueSize[name] === 0)
	{
		return;
	}
	
	nt_AppMessageQueueRunning[name] = true;

	nt_NextAppMessageQueue(name);
}

function sendAppMessageEx(name, messageObject)
{
	////console.log("sendAppMessageEx: " + nt_AppMessageQueueSize);

	nt_AppMessageQueueSize[name]++;
	nt_AppMessageQueue[name].push(messageObject);

	nt_BeginAppMessageQueue(name);
}

nt_InitAppMessageQueue(SUBREDDIT_QUEUE);
nt_InitAppMessageQueue(SUBREDDITLIST_QUEUE);
nt_InitAppMessageQueue(THREAD_QUEUE);
nt_InitAppMessageQueue(NET_IMAGE_QUEUE);
nt_InitAppMessageQueue(OTHER_QUEUE);

/********************************************************************************/

function GetThreadID(index)
{
	return loadedThreads[index].id;
}

function GetThreadName(index)
{
	return loadedThreads[index].name;
}

function GetThreadURL(index)
{
	return loadedThreads[index].url;
}

/********************************************************************************/

function RedditAPI(url, postdata, success, failure, method)
{
	//console.log("RedditAPI: Loading... " + url);

	var req = new XMLHttpRequest();

	var method = method || "POST";

	req.open(method, url, true);
	req.setRequestHeader("Content-type", "application/x-www-form-urlencoded");
	req.setRequestHeader("User-Agent", "Pebble Rebble App 1.1");

	req.onload = function(e)
	{
		if (req.readyState === 4 && req.status === 200)
		{
			//console.log("RedditAPI: Loaded " + url);
			success(req.responseText);
			return;
		}

		//console.log("RedditAPI: Failed to load " + url);
		failure(req.responseText);
	};

	req.send(postdata);
}

/********************************************************************************/

function Login()
{
	//console.log("Login");

	RedditAPI("https://ssl.reddit.com/api/login", "user=" + encodeURIComponent(username) + "&passwd=" + encodeURIComponent(password) + "&api_type=json",
		function(responseText)
		{
			var response = JSON.parse(responseText);
			modhash = response["json"]["data"]["modhash"];
			sendAppMessageEx(OTHER_QUEUE, {"ready": 1});
		},
		function(responseText)
		{
			//console.log("Login failed");
			//console.log(responseText);
			Pebble.showSimpleNotificationOnPebble("Rebble", "Failed to login to reddit. Please check your username and password in the settings dialog.");
		}
	);
}

function Logout()
{
	modhash = "";
	sendAppMessageEx(OTHER_QUEUE, {"ready": 0});
}

function Thread_Vote(id, dir)
{
	//console.log("Thread_Vote");

	if(modhash.length === 0)
	{
		return;
	}

	RedditAPI("https://ssl.reddit.com/api/vote", "dir=" + encodeURIComponent(dir) + "&id=" + encodeURIComponent(GetThreadName(id)) + "&uh=" + modhash + "&api_type=json",
		function(responseText)
		{
			//console.log(responseText);
		},
		function(responseText)
		{

		}
	);
}

function Thread_Save(id)
{
	//console.log("Thread_Save");

	if(modhash.length === 0)
	{
		return;
	}

	RedditAPI("https://ssl.reddit.com/api/save", "uh=" + modhash + "&id=" + encodeURIComponent(GetThreadName(id)) + "&api_type=json",
		function(responseText)
		{
			//console.log(responseText);
		},
		function(responseText)
		{

		}
	);
}

function SubredditList_Send(res)
{
	var sorted = res.sort(function(a, b)
	{
		return a.toLowerCase().localeCompare(b.toLowerCase());
	});

	var huge_message = "";

	for (var i = 0; i < sorted.length; ++i)
	{
		var url = sorted[i];

		if((huge_message.length + url.length + 32) > chunkSize)
		{
			sendAppMessageEx(SUBREDDITLIST_QUEUE, { "user_subreddit": huge_message });
			huge_message = "";
		}

		huge_message += url + ",";
	}

	huge_message += ";";

	sendAppMessageEx(SUBREDDITLIST_QUEUE, { "user_subreddit": huge_message });
}

function SubredditList_Load()
{
	//console.log("SubredditList_Load");

	nt_InitAppMessageQueue(SUBREDDITLIST_QUEUE);

	if(subreddits_enabled === true)
	{
		//console.log("Sending custom list");

		var res = subreddits.split(",");

		for (var i = 0; i < res.length; ++i)
		{
			res[i] = res[i].replace(/;/g, '').trim();
		}

		SubredditList_Send(res);

		return;
	}

	if(modhash.length === 0)
	{
		//console.log("Sending default list");
		SubredditList_Send(default_subreddits.split(","));
		return;
	}

	RedditAPI("https://ssl.reddit.com/subreddits/mine/subscriber.json", "uh=" + modhash + "&api_type=json",
		function(responseText)
		{
			var response = JSON.parse(responseText);

			var children = response["data"]["children"];

			var res = [];

			for (var i = 0; i < children.length; ++i)
			{
				var child = children[i]["data"];

				var url = child["url"].substr(3, child["url"].length - 4);

				res.push(url);
			}

			SubredditList_Send(res);
		},
		function(responseText)
		{
			SubredditList_Load();
		}
	);
}

function Thread_Load(subreddit, id, index)
{
	//console.log("Thread_Load: " + id);

	nt_InitAppMessageQueue(THREAD_QUEUE);

	var url;
	if(subreddit === "")
	{
		url = "http://www.reddit.com/comments/" + id + ".json";
	}
	else
	{
		url = "http://www.reddit.com/r/" + subreddit + "/comments/" + id + ".json";
	}

	RedditAPI(url, null,
		function(responseText)
		{
			var response = JSON.parse(responseText);
			if (response[0].data)
			{
				var children = response[0].data.children;

				var child = children[0].data;
				var is_self = child["is_self"];

				if(is_self)
				{
					var selftext = child["selftext"];

					//console.log("Text Length: " + selftext.length);

					var trimmed_body = selftext.replace(/[^A-Za-z 0-9 \.,\?""!@#\$%\^&\*\(\)-_=\+;:<>\/\\\|\}\{\[\]`~]*/g, '').substr(0, chunkSize - 32).trim();

					if(trimmed_body.length === 0)
					{
						trimmed_body = "Empty self post";
					}

					sendAppMessageEx(THREAD_QUEUE, {"id": index, "thread_body" : trimmed_body});
				}
			}
			else
			{
				//console.log("no data body");
			}
		},
		function(responseText)
		{

		},
		"GET"
	);
}

function Subreddit_Load(subreddit, after)
{
	//console.log("Subreddit_Load: " + subreddit);
	//console.log("Heyyy");

	nt_InitAppMessageQueue(SUBREDDIT_QUEUE);

	var url;
	if(subreddit === "")
	{
		url = "http://www.reddit.com/hot.json?limit=100";
	}
	else
	{
		url = "http://www.reddit.com/r/" + subreddit + "/hot.json?limit=100";
	}

	if(after !== undefined)
	{
		url += "&after=" + encodeURIComponent(after);
	}

	RedditAPI(url, null,
		function(responseText)
		{
			threads = 0;
			loadedThreads = {};

			var response = false;
			try
			{
				response = JSON.parse(responseText);
			}
			catch(e)
			{
				
			}

			if (response !== false && response.data)
			{
				var children = response.data.children;			

				for (var i = 0; i < children.length; ++i)
				{
					var thread = children[i].data;

					var image = "";
					var url = thread.url;
					if(url.slice(-4) === ".jpg" || url.slice(-5) === ".jpeg" || url.slice(-4) === ".gif" || url.slice(-4) === ".png" || url.slice(-4) === ".bmp")
					{
						////console.log("found image: " + url);
						image = url;
					}

					if(thread.is_self === true || image !== "")
					{
						success = true;

						loadedThreads[threads] = {
							"id": thread.id,
							"url": thread.url,
							"name": thread.name
						};

						var trimmed_title = thread.title.replace(/[^A-Za-z 0-9 \.,\?""!@#\$%\^&\*\(\)-_=\+;:<>\/\\\|\}\{\[\]`~]*/g, '');

						sendAppMessageEx(SUBREDDIT_QUEUE, {
							"title": trimmed_title,
							"score": thread.score.toString(),
							"type": thread.is_self ? 0 : 1
						});

						threads++;

						if(threads >= 20)
						{
							//console.log("Stopped at 20");
							break;
						}
					}
				}

				if(threads === 0)
				{
					sendAppMessageEx(SUBREDDIT_QUEUE, {
						"title": "No threads found",
						"score": "",
						"type": 254
					});
				}
				else
				{
					// tells the app it's done loading the current thread list
					sendAppMessageEx(SUBREDDIT_QUEUE, {
						"type": 255
					});
				}
			}
			else
			{
				sendAppMessageEx(SUBREDDIT_QUEUE, {
					"title": "Invalid Subreddit",
					"score": "",
					"type": 254
				});
			}
		},
		function(responseText)
		{
			//Subreddit_Load(subreddit);
			sendAppMessageEx(SUBREDDIT_QUEUE, {
				"title": "Error Loading Subreddit",
				"score": "",
				"type": 254
			});
		},
		"GET"
	);
}

/********************************************************************************/

Pebble.addEventListener("ready", function(e)
{
	//console.log("ready " + e.ready);

	username = localStorage.getItem("username");
	password = localStorage.getItem("password");
	subreddits_enabled = localStorage.getItem("subreddits_enabled");
	subreddits = localStorage.getItem("subreddits");

	last_subreddit = localStorage.getItem("last_subreddit");

	modhash = "";

	if(last_subreddit === undefined || last_subreddit === null || last_subreddit === false || last_subreddit === 0 || last_subreddit === "0")
	{
		last_subreddit = "";
	}

	if(username === undefined || username === null)
	{
		username = "";
	}

	if(password === undefined || password === null)
	{
		password = "";
	}

	if(subreddits_enabled === undefined || subreddits_enabled === null)
	{
		subreddits_enabled = false;
	}

	if(subreddits_enabled === 'true')
	{
		subreddits_enabled = true;
	}

	if(subreddits_enabled === 'false')
	{
		subreddits_enabled = false;
	}

	if(subreddits === undefined || subreddits === null)
	{
		subreddits = "";
	}

	sendAppMessageEx(OTHER_QUEUE, {"ready": 0});

	if(username && password && username.length > 0 && password.length > 0)
	{
		Login();
	}

	Subreddit_Load(last_subreddit);
});

Pebble.addEventListener("appmessage", function(e)
{
	////console.log(JSON.stringify(e));
	if("chunk_size" in e.payload)
	{
		chunkSize = e.payload['chunk_size'];
		//console.log("Got chunkSize");
	}
	else if ("NETIMAGE_URL" in e.payload)
	{
		var url = encodeURIComponent(GetThreadURL(e.payload['NETIMAGE_URL']));

		transferInProgress = true;
		transferInProgressURL = url;

		//SendImage("http://core.binghamton.edu:2635/?url=" + url, chunkSize);
		SendImage("http://garywilber.com:2635/?url=" + url, chunkSize);
	}
	else if ("subreddit" in e.payload)
	{
		var check = e.payload.subreddit.trim();
		if(check === "0")
		{
			Subreddit_Load(last_subreddit);
		}
		else
		{
			last_subreddit = check;
			localStorage.setItem("last_subreddit", last_subreddit);
			Subreddit_Load(last_subreddit);
		}
	}
	else if ("subreddit_next" in e.payload)
	{
		Subreddit_Load(last_subreddit, GetThreadName(threads - 1));
	}
	else if ("thread" in e.payload)
	{
		Thread_Load(last_subreddit, GetThreadID(e.payload.thread), e.payload.thread);
	}
	else if ("upvote" in e.payload)
	{
		Thread_Vote(e.payload.upvote, 1);
	}
	else if ("downvote" in e.payload)
	{
		Thread_Vote(e.payload.downvote, -1);
	}
	else if("save" in e.payload)
	{
		Thread_Save(e.payload.save);
	}
	else if("load_subredditlist" in e.payload)
	{
		SubredditList_Load();
	}
	else
	{
		//console.log("Bad Message");
		//console.log(JSON.stringify(e));
	}
});

// https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/encodeURIComponent
function fixedEncodeURIComponent(str)
{
	return encodeURIComponent(str).replace(/[!'()]/g, escape).replace(/\*/g, "%2A");
}

Pebble.addEventListener("showConfiguration", function(e)
{
	//console.log("showConfiguration");

	var url = "https://spacetech.github.io/Rebble/index.html?";

	if(username)
	{
		url += fixedEncodeURIComponent("username") + "=" + fixedEncodeURIComponent(username) + "&";
	}
	
	if(password)
	{
		url += fixedEncodeURIComponent("password") + "=" + fixedEncodeURIComponent(password) + "&";
	}

	if(subreddits_enabled)
	{
		url += fixedEncodeURIComponent("subreddits_enabled") + "=" + fixedEncodeURIComponent(subreddits_enabled) + "&";
	}

	if(subreddits)
	{
		url += fixedEncodeURIComponent("subreddits") + "=" + fixedEncodeURIComponent(subreddits) + "&";
	}

	url += "dev";

	Pebble.openURL(url);
});

Pebble.addEventListener("webviewclosed", function(e)
{
	//console.log("webviewclosed");

	if(e.response === "" || e.response === "CANCELLED")
	{
		return;
	}

	//console.log(e.response);

	var options = JSON.parse(decodeURIComponent(e.response));

	if(options["username"] === undefined)
	{
		return;
	}

	subreddits_enabled = options["subreddits_enabled"];
	if(subreddits_enabled === 'true')
	{
		subreddits_enabled = true;
	}
	if(subreddits_enabled === 'false')
	{
		subreddits_enabled = false;
	}

	subreddits = options["subreddits"];

	if(username !== options["username"] || password !== options["password"])
	{
		username = options["username"];
		password = options["password"];

		if(username.length === 0 || password.length === 0)
		{
			Logout();
		}
		else
		{
			Login();
		}
	}

	localStorage.setItem("username", username);
	localStorage.setItem("password", password);
	localStorage.setItem("subreddits_enabled", subreddits_enabled);
	localStorage.setItem("subreddits", subreddits);
});

/********************************************************************************/

function SendImage(url, chunkSize)
{
	////console.log("SendImage: " + chunkSize);

	if(chunkSize === 0)
	{
		return;
	}

	nt_InitAppMessageQueue(NET_IMAGE_QUEUE);

	var req = new XMLHttpRequest();
	req.open("GET", url, true);
	req.responseType = "arraybuffer";
	req.onload = function(e)
	{
		var buf = req.response;
		if(req.status == 200 && buf)
		{
			var byteArray = new Uint8Array(buf);

			var bytes = [];
			for(var i=0; i < byteArray.byteLength; i++)
			{
				bytes.push(byteArray[i]);
			}

			////console.log("Queuing image with " + byteArray.length + " bytes.");
			
			sendAppMessageEx(NET_IMAGE_QUEUE, {"NETIMAGE_BEGIN": bytes.length});

			for(var i=0; i < bytes.length; i += chunkSize)
			{
				sendAppMessageEx(NET_IMAGE_QUEUE, {"NETIMAGE_DATA": bytes.slice(i, i + chunkSize)});
			}

			sendAppMessageEx(NET_IMAGE_QUEUE, {"NETIMAGE_END": "done"});

			////console.log("Queued image");
		}
		else
		{
			//console.log("Request status is " + req.status);
			sendAppMessageEx(NET_IMAGE_QUEUE, {"NETIMAGE_BEGIN": 0});
			sendAppMessageEx(NET_IMAGE_QUEUE, {"NETIMAGE_END": "done"});
		}
	}
	req.onerror = function(e)
	{
		//SendImage(url, chunkSize);
		sendAppMessageEx(NET_IMAGE_QUEUE, {"NETIMAGE_BEGIN": 0});
		sendAppMessageEx(NET_IMAGE_QUEUE, {"NETIMAGE_END": "done"});		
	}

	req.send(null);
}
