# powerMeter-ESP8266
Non-invasive power meter with ESP8266
## Steps
You should have a PHP and PostgreSQL server on hand.

### Circuit
Bought a Jack female connector, and solder all it to a breadboard, and put on the bottom of the ESP8266 as a shield.
#### Modifications
- With SCT-013-030, which has a burden resistor already, so I doesn't use any. 
- Use the 3.3V voltage from the 8266, not the 5V!

![Source: openenergymonitor.org](https://camo.githubusercontent.com/62408ef6f7b5f0b6f1cc80e8207c54d8a0bf3977988a32b51f44560adfade0ea/68747470733a2f2f6f70656e656e657267796d6f6e69746f722e6f72672f666f72756d2d617263686976652f73697465732f64656661756c742f66696c65732f41726475696e6f253230414325323063757272656e74253230696e707574253230412e706e67)

Source: [openenergymonitor.org](https://openenergymonitor.org)
### Calibration
The measurements were not accurate. I used a clamp meter and various devices (notebook charger, hair dryer, kettle to create a dataset, then I fitted the actual values with [wolframalpha.com](https://wolframalpha.com). This is probably the most complicated way, you can do it otherwise. It worked me.

### Code for the ESP8266
You can find the code in [powerMeter.ino](powerMeter.ino)

### Creating database tables
```
CREATE TABLE "logger" (
	"id" INTEGER NOT NULL DEFAULT 'nextval(''logger_id_seq''::regclass)',
	"timestamp" TIMESTAMP NULL DEFAULT NULL,
	"amp" DOUBLE PRECISION NULL DEFAULT NULL,
	PRIMARY KEY ("id")
);
CREATE TABLE "price" (
	"price" INTEGER NULL DEFAULT NULL
);
UPDATE price SET price = 1.2;

CREATE VIEW "watt" AS  SELECT logger.id,
    (logger."timestamp" + '02:00:00'::interval) AS "timestamp",
    logger.amp,
    round((logger.amp * (VOLTAGE_IN_YOUR_COUNTRY)::double precision)) AS watt
   FROM logger
  ORDER BY logger.id DESC; ;
  
  
CREATE VIEW "daily_usage" AS  SELECT (date_trunc('day', watt."timestamp"))::date AS date_,
    round((avg(watt.watt) * (((count(date_trunc('hour', watt."timestamp")) / 60) + 1)))) AS watt,
    round((((avg(watt.watt) * (((count(date_trunc('hour', watt."timestamp")) / 60) + 1))) / (1000)) * (( SELECT price.price
           FROM price)))) AS price,
    (round((avg(watt.watt) * (((count(date_trunc('hour', watt."timestamp")) / 60) + 1)))) / (1000)) AS kw
   FROM watt
  GROUP BY ((date_trunc('day', watt."timestamp"))::date)
  ORDER BY ((date_trunc('day', watt."timestamp"))::date);
```
