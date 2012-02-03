


-------------------------------------------------------------
-- start: high level math
-------------------------------------------------------------
-- das ist ein test
select 1=1;
-------------------------------------------------------------
-- end: high level math
-------------------------------------------------------------

-------------------------------------------------------------
-- start: nothing -- should get removed
-------------------------------------------------------------

-------------------------------------------------------------
-- end: nothing -- should get removed
-------------------------------------------------------------


-------------------------------------------------------------
-- start: creating a few tables
-------------------------------------------------------------

create table products (
    product_id serial primary key,
    name text,
    price double precision
);

create table orders (
    order_id serial primary key,
    client_name text
);

create table orders_products (
    order_id integer references orders on delete cascade,
    product_id integer references products on delete cascade
);

-- no marker for the end of the block


-------------------------------------------------------------
-- start: fill in some useless product data
-------------------------------------------------------------

insert into products (name, price)
    select item_name || ' ' || item_number::text , random()*50 from (
            select 'dog_food' as item_name
            union
            select 'handwarmer'
            union
            select 'shoe'
            union 
            select 'wondworking tool'
        ) as items
        cross join (
            select i as item_number 
                from generate_series(1, 30) as g(i)
        ) as numbers;

insert into orders (client_name) values
    ('steve'), ('mary'), ('nobody'), ('vladimir'),
    ('jenny'), ('anna'), ('andrea'), ('luigi');


insert into orders_products (order_id, product_id)
    select order_id, product_id
        from products
        cross join orders
        where product_id%order_id=0;


----
-- start: drop tables again

drop table orders_products;
drop table orders;
drop table products;


-------------------------------------------------------------
-- start: failing query
-------------------------------------------------------------

select '344' = '344';
select now() = '344';


-------------------------------------------------------------
-- start: stuff
-------------------------------------------------------------

-- filler




alter table zzz add column xxx integer;


-------------------------------------------------------------
-- start: stuff 2
-------------------------------------------------------------

select * from pg_stat_activity;









