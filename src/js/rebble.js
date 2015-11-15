/*
	Rebble Javascript
*/

var username;
var password;
var password;
var subreddits_enabled;
var subreddits;
var last_subreddit;

var redditUrl = "https://www.reddit.com";
var default_subreddits = "all,AskReddit,aww,bestof,books,earthporn,explainlikeimfive,funny,games,IAmA,movies,music,news,pics,science,technology,television,todayilearned,tifu";

var modhash = "";

var nt_AppMessageQueue = [];
var nt_AppMessageQueueSize = [];
var nt_AppMessageQueueRunning = [];

var transferInProgress = false;
var transferInProgressURL = "";

var threads = 0;
var loadedThreads = {};

var threadCommentsDepth = null;

var chunkSize = 0;

var SUBREDDIT_QUEUE = "Subreddit";
var SUBREDDITLIST_QUEUE = "SubredditList";
var THREAD_QUEUE = "Thread";
var NET_IMAGE_QUEUE = "NetImage";
var COMMENT_QUEUE = "Comment";
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
	//console.log("sendAppMessageEx: " + nt_AppMessageQueueSize);

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

function GetThreadSubreddit(index)
{
	return loadedThreads[index].subreddit;
}

/********************************************************************************/

function RedditAPI(url, postdata, success, failure, method)
{
	//console.log("RedditAPI: Loading... " + url);

	var req = new XMLHttpRequest();

	var method = method || "POST";

	req.open(method, redditUrl + "/" + url, true);
	req.setRequestHeader("Content-type", "application/x-www-form-urlencoded");
	req.setRequestHeader("User-Agent", "Pebble Rebble App 1.3");
	req.setRequestHeader("X-Modhash", modhash)

	req.onload = function(e)
	{
		if (req.readyState === 4 && req.status === 200)
		{
			// console.log("RedditAPI: Loaded " + url);
			success(req.responseText);
			return;
		}

		// console.log("RedditAPI: Failed to load " + url + ", " + req.status);

		failure(req.responseText);
	};

	req.send(postdata);
}

/********************************************************************************/

function IsLoggedIn()
{
	return modhash.length !== 0;
}

function SetLoggedIn(mh, refresh)
{
	//console.log("SetLoggedIn! " + mh + ", " + refresh);

	modhash = mh;

	// order matters
	if(refresh)
	{
		sendAppMessageEx(OTHER_QUEUE, {"ready": 2});
	}

	sendAppMessageEx(OTHER_QUEUE, {"ready": 1});
}

function RemoveLoggedIn(refresh)
{
	//console.log("RemoveLoggedIn: " + refresh);

	modhash = "";

	// order matters
	if(refresh)
	{
		sendAppMessageEx(OTHER_QUEUE, {"ready": 2});
	}

	sendAppMessageEx(OTHER_QUEUE, {"ready": 0});
}

function CheckLogin()
{
	//console.log("CheckLogin");

	// check if we are already logged in
	RedditAPI("api/me.json", "",
		function(responseText)
		{
			var response = JSON.parse(responseText);
			if("data" in response)
			{
				SetLoggedIn(response["data"]["modhash"], false);
			}
			else
			{
				// we aren't logged in
				Login();
			}
		},
		function(responseText)
		{
			// we aren't logged in
			Login();
		}, "GET"
	);
}

function Login()
{
	//console.log("Login");

	RedditAPI("api/login", "user=" + encodeURIComponent(username) + "&passwd=" + encodeURIComponent(password) + "&api_type=json",
		function(responseText)
		{
			var response = JSON.parse(responseText);
			if("data" in response["json"] && "modhash" in response["json"]["data"])
			{
				//console.log(response["json"]["data"]);
				SetLoggedIn(response["json"]["data"]["modhash"], true);
			}
			else
			{
				//console.log(responseText);
				Pebble.showSimpleNotificationOnPebble("Rebble", "Failed to login to reddit. Please check your username and password in the settings dialog.");
			}
		},
		function(responseText)
		{
			//console.log("Login failed");
			//console.log(responseText);
			Pebble.showSimpleNotificationOnPebble("Rebble", "Failed to login to reddit. Please check your username and password in the settings dialog.");
		}
	);
}

function Logout(onLogout)
{
	//console.log("Logout");

	if(!IsLoggedIn())
	{
		if(onLogout)
		{
			onLogout();
		}
		return;
	}

	var ret = function(responseText) {
		if(onLogout)
		{
			RemoveLoggedIn(false);
			onLogout();
		}
		else
		{
			RemoveLoggedIn(true);
		}
	};

	RedditAPI("logout", "top=off&uh=" + modhash, ret, ret);
}

function Thread_Vote(id, dir)
{
	//console.log("Thread_Vote");

	if(!IsLoggedIn())
	{
		return;
	}

	RedditAPI("api/vote", "dir=" + encodeURIComponent(dir) + "&id=" + encodeURIComponent(GetThreadName(id)) + "&uh=" + modhash + "&api_type=json",
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

	if(!IsLoggedIn())
	{
		return;
	}

	RedditAPI("api/save", "uh=" + modhash + "&id=" + encodeURIComponent(GetThreadName(id)) + "&api_type=json",
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

	if(!IsLoggedIn())
	{
		//console.log("Sending default list");
		SubredditList_Send(default_subreddits.split(","));
		return;
	}

	RedditAPI("subreddits/mine/subscriber.json", "",
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
		},
		"GET"
	);
}

function Thread_Load(subreddit, id, index)
{
	//console.log("Thread_Load: " + id);

	nt_InitAppMessageQueue(THREAD_QUEUE);

	var url;
	if(subreddit === "")
	{
		url = "comments/" + id + ".json";
	}
	else
	{
		url = "r/" + subreddit + "/comments/" + id + ".json";
	}

	threadCommentsDepth = null;

	RedditAPI(url, null,
		function(responseText)
		{
			var response = JSON.parse(responseText);

			var original_post = response[0].data
			if (original_post)
			{
				var children = original_post.children;

				var child = children[0].data;
				var is_self = child["is_self"];

				if(is_self)
				{
					var selftext = child["selftext"];

					//console.log("Text Length: " + selftext.length);

					var trimmed_body = selftext.replace(/[^A-Za-z 0-9 \.,\?""!@#\$%\^&\*\(\)-_=\+;:<>\/\\\|\}\{\[\]`~]*/g, '').substr(0, chunkSize - 128).trim();

					if(trimmed_body.length === 0)
					{
						trimmed_body = "Empty self post";
					}

					sendAppMessageEx(THREAD_QUEUE, {
						"id": index,
						"title": child["author"],
						"thread_body" : trimmed_body
					});
				}

				//console.log("setting comments");

				threadCommentsDepth = [{
					"index": 0,
					"data": response[1].data
				}];
			}
			else
			{
				//console.log("Thread_Load no data body");
			}
		},
		function(responseText)
		{
			//console.log("Thread_Load Failed!");
			//console.log(responseText);
		},
		"GET"
	);
}

function Comments_Load(dir)
{
	//console.log("Comments_Load: " + dir);

	nt_InitAppMessageQueue(COMMENT_QUEUE);

	if(threadCommentsDepth === null)
	{
		// we are probably viewing an image. additional loading must be done.
		LoadImageComments(dir);
		return;
	}

	var depth = threadCommentsDepth.length - 1;

	comments = threadCommentsDepth[depth];

	//console.log("Current Depth: " + depth + ", # of comments: " + comments.data.children.length);

	if(dir == 0)
	{
		// next comment for the current depth
		comments.index++;
	}
	else if(dir == 1)
	{
		// previous comment for the current depth
		comments.index--;
	}
	else if(dir == 2)
	{
		// go deeper

		if(comments.data.children[comments.index].data.replies === "")
		{
			return;
		}

		threadCommentsDepth.push({
			"index": 0,
			"data": comments.data.children[comments.index].data.replies.data
		});

		Comments_Load(-1);

		return;
	}
	else if(dir == 3)
	{
		// go towards the surface!

		threadCommentsDepth.pop();

		Comments_Load(-1);
		
		return;
	}

	var max_comments = 0;
	for(var i=0; i < comments.data.children.length; i++)
	{
		if(comments.data.children[i].kind === "t1")
		{
			max_comments++;
		}
	}

	if(comments.index < 0 || comments.index >= max_comments)
	{
		//console.log("No comments");
		sendAppMessageEx(COMMENT_QUEUE, {"comment": "No more comments here"});
	}
	else
	{
		var commentData = comments.data.children[comments.index].data;

		var replies = commentData.replies;
		var author = commentData.author;
		var body = commentData.body;

		var score = "Hidden";
		if(!commentData.score_hidden)
		{
			score = commentData.ups - commentData.downs;
			score = score.toString();
		}

		// count next depths comments
		var count = 0;
		if(replies !== "")
		{
			for(var i=0; i < replies.data.children.length; i++)
			{
				if(replies.data.children[i].kind === "t1")
				{
					count++;
				}
			}
		}

		// it looks weird because I'm reusing keys.
		sendAppMessageEx(COMMENT_QUEUE, {
			"title": author,
			"score": score,
			"comment": body.substr(0, chunkSize - 128),

			// current depth
			"type": depth,

			// current index
			"id": comments.index,

			// max comments for current depth
			"thread_body": max_comments,

			// can we go deeper
			"user_subreddit": count > 0 ? 1 : 0
		});

		//console.log("SENT " + score + " " + body.length);
		//console.log("blah " + (count > 0 ? "next depth possible" : "end of the line"));
		//console.log(body);
	}
}

function Subreddit_Load(subreddit, after)
{
	//console.log("Subreddit_Load: " + subreddit);
	//console.log("Heyyy");

	nt_InitAppMessageQueue(SUBREDDIT_QUEUE);

	var url;
	var frontpage = false;

	if(subreddit === "")
	{
		frontpage = true;
		url = "hot.json?limit=100";
	}
	else
	{
		url = "r/" + subreddit + "/hot.json?limit=100";
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
						//console.log("found image: " + url);
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

						var messageObject = {
							"title": trimmed_title,
							"score": thread.score.toString(),
							"type": thread.is_self ? 0 : 1
						}

						if(frontpage)
						{
							messageObject["thread_subreddit"] = thread.subreddit;
							loadedThreads[threads].subreddit = thread.subreddit;
						}
						else
						{
							loadedThreads[threads].subreddit = "";
						}

						//console.log("loadedThreads " + threads + ", " + thread.title);

						sendAppMessageEx(SUBREDDIT_QUEUE, messageObject);

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
	console.log("ready " + e.ready);

	username = localStorage.getItem("username");
	password = localStorage.getItem("password");
	subreddits_enabled = localStorage.getItem("subreddits_enabled");
	subreddits = localStorage.getItem("subreddits");
	last_subreddit = localStorage.getItem("last_subreddit");

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
		CheckLogin();
	}

	Subreddit_Load(last_subreddit);
});

Pebble.addEventListener("appmessage", function(e)
{
	//console.log(JSON.stringify(e.payload));
try
{
	if("chunk_size" in e.payload)
	{
		chunkSize = e.payload['chunk_size'];
		//console.log("Got chunkSize");
	}

	if ("NETIMAGE_URL" in e.payload)
	{
		threadCommentsIndex = e.payload['NETIMAGE_URL'];

		var url = GetThreadURL(e.payload['NETIMAGE_URL']);

		transferInProgress = true;
		transferInProgressURL = url;

		//SendImage("http://core.binghamton.edu:2635/?url=" + url, chunkSize);
		//SendImage("http://garywilber.com:2635/?url=" + url, chunkSize - 8);
		SendImage(url, chunkSize - 8);
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
	else if("load_comments" in e.payload)
	{
		Comments_Load(e.payload.load_comments);
	}
	else
	{
		//console.log("Bad Message");
		//console.log(JSON.stringify(e));
	}
}
catch(ex)
{
	console.log("Error Detected");
	console.log(ex);
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
			Logout(null);
		}
		else
		{
			Logout(Login);
		}
	}

	localStorage.setItem("username", username);
	localStorage.setItem("password", password);
	localStorage.setItem("subreddits_enabled", subreddits_enabled);
	localStorage.setItem("subreddits", subreddits);
});

/********************************************************************************/

function LoadImageComments(dir)
{
	threadCommentsDepth = null;

	var url;
	if(GetThreadSubreddit(threadCommentsIndex) === "")
	{
		url = "comments/" + GetThreadID(threadCommentsIndex) + ".json";
	}
	else
	{
		url = "r/" + GetThreadSubreddit(threadCommentsIndex) + "/comments/" + GetThreadID(threadCommentsIndex) + ".json";
	}
	
	RedditAPI(url, null,
		function(responseText)
		{
			var response = JSON.parse(responseText);

			var original_post = response[0].data
			if (original_post)
			{
				threadCommentsDepth = [{
					"index": 0,
					"data": response[1].data
				}];

				if(dir !== null)
				{
					Comments_Load(dir);
				}
			}
		},
		function(responseText)
		{

		},
		"GET"
	);
}

function SendImage(url, chunkSize)
{
	//console.log("SendImage: " + chunkSize);

	if(chunkSize === 0)
	{
		return;
	}

	LoadImageComments(null);

	nt_InitAppMessageQueue(NET_IMAGE_QUEUE);

	getImage(url, chunkSize);
}