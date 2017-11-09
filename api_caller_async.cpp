#include "api_caller.h"
#include "user_center/t_user_info.h"
#include "encrypt/url_util.h"
#include "HttpsClient.h"

int	ApiCaller::AsyncCallGet(std::string &path_, std::map< std::string, std::string> &param_map_, const ASYNSC_API_CALLBACK& response_cb, bool need_url_encode)
{
	for (auto i = param_map_.begin(); need_url_encode && i != param_map_.end(); i++)
	{
		string& v = i->second;
		v = urlencode(v);
	}
	std::string url = assemble_general_url_(path_, param_map_);

	auto  res_cb = [=](const CHttpRequestPtr & http_request)
	{
		ApiResult api_result;
		if (!http_request || http_request->m_response.empty())
		{
			api_result.SetCode(-1);
			api_result.SetMsg(L"连接服务器失败");
		}
		else
		{
			ApiResult::Parse(http_request->m_response, api_result);
			if (api_result.Success())
			{
				if (url.find("/demand/list") == string::npos)
				{
					QLOG_APP(L"aysnc get api {0} result ok") << url.c_str();
				}

			}
			else
			{
				QLOG_APP(L"aysnc get api {0} result {1}") << url.c_str() << http_request->m_response;
			}
		}

		if (response_cb)
		{
			Post2UI(nbase::Bind(response_cb, api_result));
		}
	};

	curl_slist* sl_headers = NULL;
	sl_headers = curl_slist_append(sl_headers, "Accept: */*");
	sl_headers = curl_slist_append(sl_headers, builde_cookie_().c_str());
	string ts_str = "xy-nonce: " + param_map_["ts"];
	sl_headers = curl_slist_append(sl_headers, ts_str.c_str());
	string sign_str = "xy-sign: " + param_map_["sign"];
	sl_headers = curl_slist_append(sl_headers, sign_str.c_str());
	sl_headers = curl_slist_append(sl_headers, "xy-access-key: jyxb");

	int status = CHttpClient::GetInstance()->asyncGet(url, sl_headers, res_cb);

	return status;
}

int	ApiCaller::AsyncCallPost(std::string &path_, std::map< std::string, std::string> &param_map_, const ASYNSC_API_CALLBACK& response_cb, bool need_url_encode)
{
	for (auto i = param_map_.begin(); need_url_encode && i != param_map_.end(); i++)
	{
		string& v = i->second;
		v = urlencode(v);
	}

	std::string post_f_ = generate_post_field_(param_map_);

	SS_MAP field_map_;
	field_map_["_post_field_"] = post_f_;

	std::string url = assemble_post_url_(path_, field_map_);
	/*std::string ua = build_user_agent_();*/

	auto  res_cb = [=](const CHttpRequestPtr & http_request)
	{
		ApiResult api_result;

#ifdef DEBUG
		QLOG_APP(nbase::UTF8ToUTF16(http_request->m_response));
#endif


		if (!http_request || http_request->m_response.empty())
		{
			api_result.SetCode(-1);
			api_result.SetMsg(L"连接服务器失败");
		}
		else
		{
			ApiResult::Parse(http_request->m_response, api_result);
			if (api_result.Success())
			{
				QLOG_APP(L"aysnc post api {0} result ok") << url.c_str();

			}
			else
			{
				QLOG_APP(L"aysnc post api {0} result {1}") << url.c_str() << http_request->m_response;
			}
		}
		if (response_cb)
		{
			Post2UI(nbase::Bind(response_cb, api_result));
		}
	};


	curl_slist* sl_headers = NULL;
	sl_headers = curl_slist_append(sl_headers, "Accept: */*");
	sl_headers = curl_slist_append(sl_headers, builde_cookie_().c_str());
	sl_headers = curl_slist_append(sl_headers, "Expect:");
	string ts_str = "xy-nonce: " + param_map_["ts"];
	sl_headers = curl_slist_append(sl_headers, ts_str.c_str());
	string sign_str = "xy-sign: " + param_map_["sign"];
	sl_headers = curl_slist_append(sl_headers, sign_str.c_str());
	sl_headers = curl_slist_append(sl_headers, "xy-access-key: jyxb");

	LONG64 status = CHttpClient::GetInstance()->asyncPost(url, post_f_, sl_headers, res_cb);


	//HttpRequest http_request_(url, post_f_.c_str(), post_f_.size(), res_cb);
	//std::string app_key = "213965906950646";
	//http_request_.AddHeader("Accept", "*/*");
	//http_request_.AddHeader("Content-Type", "application/x-www-form-urlencoded");
	//http_request_.AddHeader("User-Agent", ua);
	//http_request_.AddHeader("Cookie", UserInfo::GetInstance()->GetLoginCookie());
	//http_request_.AddHeader("xy-nonce", field_map_["ts"]);
	//http_request_.AddHeader("xy-sign", field_map_["sign"]);
	//http_request_.AddHeader("xy-access-key", "jyxb");
	////http_request_.AddHeader("appKey", app_key);
	//http_request_.SetMethodAsPost();
	//http_request_.SetTimeout(30000);

	//return PostRequest(http_request_);
	return (int)status;
}