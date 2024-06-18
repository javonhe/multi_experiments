#include <stdio.h>  

// 定义基类  
struct Shape
{  
    void (*draw)(struct Shape*);  
};

// 定义派生类  
struct Circle
{  
    struct Shape shape;  
    int radius;  
};

struct Rectangle
{  
    struct Shape shape;  
    int width;  
    int height;  
};


void drawCircle(struct Shape *shape)
{  
    printf("Drawing a circle...\n");
    struct Circle *circle = (struct Circle *)shape;
    printf("Circle radius is %d\n", circle->radius);
}


void drawRectangle(struct Shape *shape)
{  
    printf("Drawing a rectangle...\n");
    struct Rectangle *rectangle = (struct Rectangle *)shape;
    printf("Rectangle width is %d\n", rectangle->width);
    printf("Rectangle height is %d\n", rectangle->height);
}

  
int main()
{  

    struct Circle circle;
    circle.shape.draw = drawCircle;
    circle.radius = 10;

    struct Rectangle rectangle;  
    rectangle.shape.draw = drawRectangle;
    rectangle.width = 10;
    rectangle.height = 20;

    struct Shape *shape;

    shape = (struct Shape *)&circle;
    shape->draw(shape);

    shape = (struct Shape *)&rectangle;
    shape->draw(shape);
  
    return 0;  
}