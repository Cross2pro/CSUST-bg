import requests
import re
import time
import random
import webbrowser
from colorama import init
from accounts import account_info

init(autoreset=True)

def get_user_ip():
    """获取用户IP地址"""
    print('正在获取用户IP地址...')
    try:
        url = 'https://login.csust.edu.cn/drcom/chkstatus?callback=dr1002&jsVersion=4.X&v=1611&lang=zh'
        response = requests.get(url=url, timeout=5)
        # 使用正则表达式提取v46ip字段
        ip_match = re.search(r'"v46ip":\s*"([^"]+)"', response.text)
        if ip_match:
            wlan_user_ip = ip_match.group(1)
            print(f'获取到用户IP: {wlan_user_ip}')
            return wlan_user_ip
        else:
            print('未能从响应中提取IP地址')
            return None
    except Exception as e:
        print(f'获取IP地址时出错: {e}')
        return None

def login():
    """登录校园网"""
    try:
        # 获取用户IP
        wlan_user_ip = get_user_ip()
        if not wlan_user_ip:
            print('无法获取用户IP，请检查网络连接')
            input('按任意键退出...')
            return False
            
        print('\n====================================')
        print(f'用户IP地址: {wlan_user_ip}')
        print('====================================')
        
        time.sleep(1)
        print('开始尝试登录...')
        time.sleep(1)
        
        # 随机选择一个账号
        account_in = account_info[random.randint(1, len(account_info)-1)]
        account = account_in[0]
        pswd = account_in[1]
        
        # 新的登录API
        login_url = f'https://login.csust.edu.cn:802/eportal/portal/login?callback=dr1003&login_method=1&user_account=%2C0%2C{account}&user_password={pswd}&wlan_user_ip={wlan_user_ip}&wlan_user_ipv6=&wlan_user_mac=000000000000&wlan_ac_ip=&wlan_ac_name=&jsVersion=4.2.1&terminal_type=1&lang=zh-cn&v=1250&lang=zh'
        
        print(f'使用账号: {account}')
        response = requests.get(url=login_url, timeout=5)
        
        # 检查登录结果
        if 'success' in response.text.lower():
            print('登录请求发送成功')
        else:
            print('登录请求可能未成功，正在检查网络连接...')
        
        return check()
    except Exception as e:
        print(f'登录过程中出错: {e}')
        print('请检查网络连接')
        input('按任意键退出...')
        return False

def check():
    """检查网络连接状态"""
    print('\n\n==============')
    print('\n    注意!!\n    首先连接上WiFi "csust—bg" 或者插上办公区的网线\n')
    print('本程序仅供学习用途\n')
    print('==============\n')
    print('检查网络中，请稍后...')
    
    try:
        response = requests.get('https://www.baidu.com', timeout=3)
        if response.status_code == 200:
            print('\n==============')
            print('     登录成功')
            print('    程序准备退出')
            print('==============\n')
            #webbrowser.open('https://www.baidu.com')
            print('本软件可以学习、传播，但是不能用于盈利目的！')
            input('按任意键退出...')
            return True
    except:
        pass
        
    print('断网或者连接失败，尝试重新登录...')
    return login()

if __name__ == "__main__":
    check()