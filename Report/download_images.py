import urllib.request
import urllib.parse
import json
import os

components = ['ESP-WROOM-32', 'ESP32-CAM', 'MAX30102 sensor', 'SIM900 module', 'NEO-6M GPS module', 'OLED display I2C']

for comp in components:
    try:
        search_url = f'https://commons.wikimedia.org/w/api.php?action=query&list=search&srsearch={urllib.parse.quote(comp)}&srnamespace=6&format=json'
        req = urllib.request.Request(search_url, headers={'User-Agent': 'Mozilla/5.0'})
        with urllib.request.urlopen(req) as response:
            data = json.loads(response.read().decode())
            if data['query']['search']:
                for item in data['query']['search']:
                    title = item['title']
                    if 'pdf' in title.lower(): continue
                    
                    img_url_req = f'https://commons.wikimedia.org/w/api.php?action=query&titles={urllib.parse.quote(title)}&prop=imageinfo&iiprop=url&format=json'
                    req2 = urllib.request.Request(img_url_req, headers={'User-Agent': 'Mozilla/5.0'})
                    with urllib.request.urlopen(req2) as resp2:
                        data2 = json.loads(resp2.read().decode())
                        pages = data2['query']['pages']
                        for page_id in pages:
                            img_url = pages[page_id]['imageinfo'][0]['url']
                            print(f'Found {comp}: {img_url}')
                            safe_name = comp.replace(" ", "_").replace("-", "_") + "_real.jpg"
                            
                            req3 = urllib.request.Request(img_url, headers={'User-Agent': 'Mozilla/5.0'})
                            with urllib.request.urlopen(req3) as img_response, open(safe_name, 'wb') as out_file:
                                out_file.write(img_response.read())
                            break
                    break # Only get first valid image
            else:
                print(f'No image for {comp}')
    except Exception as e:
        print(f'Error {comp}: {e}')
